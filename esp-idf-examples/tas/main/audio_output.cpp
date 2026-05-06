#include "utilities.h"
#ifdef ENABLE_AUDIO

#include "audio_output.h"
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c.h"
#include "driver/i2s_std.h"
#include "es8311.h"
#include "esp_log.h"

static const char *TAG = "AUDIO";

// I2C 핀
#define I2C_SDA_PIN 47
#define I2C_SCL_PIN 48
#define I2C_NUM I2C_NUM_0

// I2S 핀
#define I2S_MCLK_PIN 5
#define I2S_BCLK_PIN 16
#define I2S_WS_PIN   7
#define I2S_DOUT_PIN 6

// TCA9554 IO Expander
#define TCA9554_ADDR 0x20

static i2s_chan_handle_t tx_chan = NULL;
static es8311_handle_t es8311_dev = NULL;
static TaskHandle_t audio_task_handle = NULL;
static volatile bool audio_running = false; // 안전한 태스크 종료를 위한 플래그
static bool i2c_initialized = false;

// 1kHz Sine Wave Generation Task
static void audio_sine_task(void *arg) {
    ESP_LOGI(TAG, "Audio Sine Task Started");
    
    int sample_rate = 44100;
    int freq = 440; // 440Hz (A4 note) for a smoother sine wave (~100 samples/cycle)
    int samples_per_cycle = sample_rate / freq;
    int16_t *buffer = (int16_t*) malloc(samples_per_cycle * 2 * sizeof(int16_t)); // Stereo
    
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate audio buffer");
        vTaskDelete(NULL);
        return;
    }

    // Generate PCM data (10000 amplitude to avoid clipping)
    for(int i=0; i<samples_per_cycle; i++) {
        int16_t val = (int16_t)(sin(2 * M_PI * i / samples_per_cycle) * 10000);
        buffer[i*2]     = val; // Left channel
        buffer[i*2+1]   = val; // Right channel
    }

    size_t bytes_written;
    while(audio_running) {
        // i2s_channel_write에 타임아웃을 주어 플래그를 주기적으로 확인할 수 있게 함
        esp_err_t err = i2s_channel_write(tx_chan, buffer, samples_per_cycle * 2 * sizeof(int16_t), &bytes_written, 100 / portTICK_PERIOD_MS);
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "I2S Write error: %d", err);
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }

    // 루프를 빠져나오면 자원을 정리하고 태스크 스스로 종료
    free(buffer);
    audio_task_handle = NULL; // 종료되었음을 외부에 알림
    vTaskDelete(NULL);
}

void audio_output_start(void) {
    if (audio_task_handle != NULL) {
        ESP_LOGW(TAG, "Audio is already running");
        return;
    }

    ESP_LOGI(TAG, "Starting Audio Output...");

    // 1. I2C Initialization (if not already done)
    if (!i2c_initialized) {
        i2c_config_t i2c_conf = {};
        i2c_conf.mode = I2C_MODE_MASTER;
        i2c_conf.sda_io_num = I2C_SDA_PIN;
        i2c_conf.scl_io_num = I2C_SCL_PIN;
        i2c_conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
        i2c_conf.master.clk_speed = 400000;
        
        esp_err_t err = i2c_param_config(I2C_NUM, &i2c_conf);
        if (err == ESP_OK) {
            err = i2c_driver_install(I2C_NUM, i2c_conf.mode, 0, 0, 0);
        }
        
        if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
            i2c_initialized = true;
            ESP_LOGI(TAG, "I2C Initialized");
        } else {
            ESP_LOGE(TAG, "I2C Init Failed: %s", esp_err_to_name(err));
            return;
        }
    }

    // 2. Enable PA via TCA9554 (I2C addr 0x20)
    uint8_t cfg_reg[2] = {0x03, 0xFE}; // Config Reg (0x03), P0 Output (0)
    i2c_master_write_to_device(I2C_NUM, TCA9554_ADDR, cfg_reg, 2, 1000 / portTICK_PERIOD_MS);
    
    uint8_t out_reg[2] = {0x01, 0x01}; // Output Reg (0x01), P0 HIGH (1)
    i2c_master_write_to_device(I2C_NUM, TCA9554_ADDR, out_reg, 2, 1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Power Amplifier Enabled via TCA9554");

    // 3. Initialize ES8311 Codec
    if (es8311_dev == NULL) {
        es8311_dev = es8311_create(I2C_NUM, ES8311_ADDRRES_0);
        if (!es8311_dev) {
            ESP_LOGE(TAG, "Failed to create ES8311 device");
            return;
        }

        es8311_clock_config_t clk_cfg = {
            .mclk_inverted = false,
            .sclk_inverted = false,
            .mclk_from_mclk_pin = true,
            .mclk_frequency = 44100 * 256,
            .sample_frequency = 44100
        };

        esp_err_t err = es8311_init(es8311_dev, &clk_cfg, ES8311_RESOLUTION_16, ES8311_RESOLUTION_16);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "ES8311 Init Failed: %s", esp_err_to_name(err));
            return;
        }

        es8311_voice_volume_set(es8311_dev, 30, NULL);
        es8311_voice_mute(es8311_dev, false);
        ESP_LOGI(TAG, "ES8311 Codec Initialized");
    }

    // 4. Initialize I2S
    if (tx_chan == NULL) {
        i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
        esp_err_t err = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(err));
            return;
        }

        i2s_std_config_t std_cfg = {
            .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(44100),
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = (gpio_num_t)I2S_MCLK_PIN,
                .bclk = (gpio_num_t)I2S_BCLK_PIN,
                .ws   = (gpio_num_t)I2S_WS_PIN,
                .dout = (gpio_num_t)I2S_DOUT_PIN,
                .din  = I2S_GPIO_UNUSED,
                .invert_flags = {
                    .mclk_inv = false,
                    .bclk_inv = false,
                    .ws_inv   = false,
                },
            },
        };
        std_cfg.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256; 

        err = i2s_channel_init_std_mode(tx_chan, &std_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to init I2S std mode: %s", esp_err_to_name(err));
            return;
        }

        err = i2s_channel_enable(tx_chan);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(err));
            return;
        }
        ESP_LOGI(TAG, "I2S Channel Enabled");
    }

    // 5. Start Sine Wave Task
    audio_running = true;
    xTaskCreate(audio_sine_task, "audio_sine", 4096, NULL, 5, &audio_task_handle);
}

void audio_output_stop(void) {
    ESP_LOGI(TAG, "Stopping Audio Output...");

    // 1. Stop Task Safely
    if (audio_task_handle != NULL) {
        audio_running = false; // 플래그를 내려서 태스크가 루프를 탈출하도록 유도
        while(audio_task_handle != NULL) {
            vTaskDelay(10 / portTICK_PERIOD_MS); // 태스크가 완전히 종료될 때까지 대기
        }
        ESP_LOGI(TAG, "Audio Task Stopped Safely");
    }

    // 2. Disable I2S
    if (tx_chan != NULL) {
        i2s_channel_disable(tx_chan);
        i2s_del_channel(tx_chan);
        tx_chan = NULL;
        ESP_LOGI(TAG, "I2S Channel Disabled");
    }

    // 3. Disable PA via TCA9554
    if (i2c_initialized) {
        uint8_t out_reg[2] = {0x01, 0x00}; // Output Reg (0x01), P0 LOW (0)
        i2c_master_write_to_device(I2C_NUM, TCA9554_ADDR, out_reg, 2, 1000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "Power Amplifier Disabled");
    }
}

void audio_output_set_volume(int volume) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    
    if (es8311_dev != NULL) {
        es8311_voice_volume_set(es8311_dev, volume, NULL);
        ESP_LOGI(TAG, "Volume set to %d", volume);
    } else {
        ESP_LOGW(TAG, "Cannot set volume: ES8311 not initialized yet.");
    }
}

#endif // ENABLE_AUDIO

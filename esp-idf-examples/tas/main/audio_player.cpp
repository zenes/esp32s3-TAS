#include "audio_player.h"
#include "audio_output.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

// Helix MP3 Decoder Headers
#include "mp3dec.h"

static const char *TAG = "AUDIO_PLAYER";

static TaskHandle_t audio_task_handle = NULL;
static volatile bool is_playing = false;
static char current_file[128];

// PSRAM을 활용한 대용량 버퍼 (스택/내부 힙 오염 방지)
static uint8_t *read_buf = NULL;
static short *pcm_buf = NULL;

void audio_player_task(void *pvParameters) {
    char *filename = (char *)pvParameters;
    char full_path[160];
    snprintf(full_path, sizeof(full_path), "/fatfs/%s", filename);

    FILE *f = fopen(full_path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", full_path);
        is_playing = false;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Starting playback: %s", full_path);
    
    HMP3Decoder hMP3Decoder = MP3InitDecoder();
    if (!hMP3Decoder) {
        ESP_LOGE(TAG, "Failed to init MP3 decoder");
        fclose(f);
        is_playing = false;
        vTaskDelete(NULL);
        return;
    }

    bool is_mp3 = (strstr(filename, ".mp3") || strstr(filename, ".MP3"));
    MP3FrameInfo frameInfo;

    if (is_mp3) {
        uint8_t *read_ptr = read_buf;
        int bytes_left = 0;

        while (is_playing) {
            // Buffer management: move remaining data to start and refill
            if (bytes_left < 2048) {
                if (bytes_left > 0) {
                    memmove(read_buf, read_ptr, bytes_left);
                }
                size_t n = fread(read_buf + bytes_left, 1, 8192 - bytes_left, f);
                if (n == 0 && bytes_left == 0) break;
                bytes_left += n;
                read_ptr = read_buf;
            }

            int offset = MP3FindSyncWord(read_ptr, bytes_left);
            if (offset < 0) {
                bytes_left = 0;
                continue;
            }
            read_ptr += offset;
            bytes_left -= offset;

            int err = MP3Decode(hMP3Decoder, &read_ptr, &bytes_left, pcm_buf, 0);
            if (err == ERR_MP3_NONE) {
                MP3GetLastFrameInfo(hMP3Decoder, &frameInfo);
                audio_output_write((uint8_t*)pcm_buf, frameInfo.outputSamps * sizeof(short));
            } else if (err == ERR_MP3_INDATA_UNDERFLOW) {
                bytes_left = 0; 
            } else {
                if (bytes_left > 0) {
                    read_ptr++;
                    bytes_left--;
                }
            }
        }
    } else {
        // WAV playback
        fseek(f, 44, SEEK_SET);
        while (is_playing) {
            size_t n = fread(read_buf, 1, 4096, f);
            if (n == 0) break;
            audio_output_write(read_buf, n);
        }
    }

    MP3FreeDecoder(hMP3Decoder);
    fclose(f);
    ESP_LOGI(TAG, "Playback finished: %s", filename);
    is_playing = false;
    audio_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t audio_player_init() {
    // PSRAM에 대용량 버퍼 미리 할당 (내부 RAM 절약 및 안전성 확보)
    if (read_buf == NULL) {
        read_buf = (uint8_t *)heap_caps_malloc(8192, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (pcm_buf == NULL) {
        // Helix needs 2 * 1152 samples minimum, 넉넉하게 8192바이트 할당
        pcm_buf = (short *)heap_caps_malloc(8192, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }

    if (!read_buf || !pcm_buf) {
        ESP_LOGE(TAG, "Failed to allocate PSRAM buffers");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t audio_player_play(const char *filename) {
    if (is_playing) {
        is_playing = false; // Stop current task
        vTaskDelay(pdMS_TO_TICKS(100)); // Wait for exit
    }

    strncpy(current_file, filename, sizeof(current_file));
    is_playing = true;
    xTaskCreate(audio_player_task, "audio_task", 8192, current_file, 5, &audio_task_handle);
    
    return ESP_OK;
}

void audio_player_stop() {
    is_playing = false;
}

bool audio_player_is_playing() {
    return is_playing;
}

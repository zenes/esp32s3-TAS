#pragma once

#define USE_LOVYANGFX
#include <LovyanGFX.hpp>

// LilyGO T-ETH-Lite-S3 ST7789 SPI Configuration
class LGFX : public lgfx::LGFX_Device
{
#if defined(TFT_PARALLEL_16_BIT)
    lgfx::Bus_Parallel16 _bus_instance;
#elif defined(TFT_PARALLEL_8_BIT)
    lgfx::Bus_Parallel8 _bus_instance;
#else
    lgfx::Bus_SPI       _bus_instance;
#endif

#ifdef LCD_TYPE_ST7789
    lgfx::Panel_ST7789  _panel_instance;
#else
    lgfx::Panel_ILI9341 _panel_instance;
#endif

public:
    LGFX(void)
    {
#if defined(TFT_PARALLEL_16_BIT) || defined(TFT_PARALLEL_8_BIT)
        {
            auto cfg = _bus_instance.config();
            cfg.port = 0; 
#if defined(TFT_PARALLEL_16_BIT)
            cfg.i2s_port = I2S_NUM_0; 
#endif
            cfg.freq_write = 20000000;
            
            // Use LGFX_ prefixed macros if available (from CMakeLists.txt)
            #if defined(LGFX_WR)
            cfg.pin_wr = LGFX_WR;
            #else
            cfg.pin_wr = LCD_WR_PIN;
            #endif

            #if defined(LGFX_RD)
            cfg.pin_rd = LGFX_RD;
            #else
            cfg.pin_rd = LCD_RD_PIN;
            #endif

            #if defined(LGFX_DC)
            cfg.pin_rs = LGFX_DC;
            #else
            cfg.pin_rs = LCD_DC_PIN;
            #endif

            #if defined(LGFX_D0)
            cfg.pin_d0 = LGFX_D0; cfg.pin_d1 = LGFX_D1; cfg.pin_d2 = LGFX_D2; cfg.pin_d3 = LGFX_D3;
            cfg.pin_d4 = LGFX_D4; cfg.pin_d5 = LGFX_D5; cfg.pin_d6 = LGFX_D6; cfg.pin_d7 = LGFX_D7;
            #else
            cfg.pin_d0 = LCD_D0_PIN; cfg.pin_d1 = LCD_D1_PIN; cfg.pin_d2 = LCD_D2_PIN; cfg.pin_d3 = LCD_D3_PIN;
            cfg.pin_d4 = LCD_D4_PIN; cfg.pin_d5 = LCD_D5_PIN; cfg.pin_d6 = LCD_D6_PIN; cfg.pin_d7 = LCD_D7_PIN;
            #endif

#if defined(TFT_PARALLEL_16_BIT)
            #if defined(LGFX_D8)
            cfg.pin_d8 = LGFX_D8; cfg.pin_d9 = LGFX_D9; cfg.pin_d10 = LGFX_D10; cfg.pin_d11 = LGFX_D11;
            cfg.pin_d12 = LGFX_D12; cfg.pin_d13 = LGFX_D13; cfg.pin_d14 = LGFX_D14; cfg.pin_d15 = LGFX_D15;
            #else
            cfg.pin_d8 = LCD_D8_PIN; cfg.pin_d9 = LCD_D9_PIN; cfg.pin_d10 = LCD_D10_PIN; cfg.pin_d11 = LCD_D11_PIN;
            cfg.pin_d12 = LCD_D12_PIN; cfg.pin_d13 = LCD_D13_PIN; cfg.pin_d14 = LCD_D14_PIN; cfg.pin_d15 = LCD_D15_PIN;
            #endif
#endif
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
#else
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;     // FSPI
            cfg.spi_mode = 0;             // SPI Mode 0
            cfg.freq_write = 40000000;    // 40MHz
            cfg.freq_read  = 16000000;    // 16MHz
            cfg.spi_3wire  = false;
            cfg.use_lock   = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = LCD_SCLK_PIN;
            cfg.pin_mosi = LCD_MOSI_PIN;
            cfg.pin_miso = LCD_MISO_PIN;
            cfg.pin_dc   = LCD_DC_PIN;

            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
#endif

        {
            auto cfg = _panel_instance.config();
            
            #if defined(LGFX_CS)
            cfg.pin_cs = LGFX_CS;
            #else
            cfg.pin_cs = LCD_CS_PIN;
            #endif

            #if defined(LGFX_RST)
            cfg.pin_rst = LGFX_RST;
            #else
            cfg.pin_rst = LCD_RST_PIN;
            #endif
            cfg.pin_busy         = -1;
            cfg.panel_width      = LCD_WIDTH;
            cfg.panel_height     = LCD_HEIGHT;
            cfg.offset_x         = 0;
            cfg.offset_y         = 0;
            cfg.offset_rotation  = 1;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits  = 1;
            cfg.readable         = true;
            cfg.invert           = true;
            cfg.rgb_order        = false;
            cfg.dlen_16bit       = false;
            cfg.bus_shared       = true;

            _panel_instance.config(cfg);
        }

        setPanel(&_panel_instance);
    }
};

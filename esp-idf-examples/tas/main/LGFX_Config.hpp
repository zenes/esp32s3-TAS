#pragma once

#ifdef USE_LOVYANGFX

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
#ifdef TFT_PARALLEL_16_BIT
  lgfx::Bus_Parallel16 _bus_instance;
#elif defined(TFT_PARALLEL_8_BIT)
  lgfx::Bus_Parallel8 _bus_instance;
#endif

#ifdef LCD_TYPE_ST7789
  lgfx::Panel_ST7789 _panel_instance;
#else
  lgfx::Panel_ILI9341 _panel_instance;
#endif

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.port = 0; // I2S 포트 (ESP32-S3에서는 I2S 또는 LCD_CAM)
      cfg.freq_write = 10000000;
      
      cfg.pin_wr = LGFX_WR;
      cfg.pin_rd = LGFX_RD;
      cfg.pin_rs = LGFX_DC;
      
      cfg.pin_d0 = LGFX_D0;
      cfg.pin_d1 = LGFX_D1;
      cfg.pin_d2 = LGFX_D2;
      cfg.pin_d3 = LGFX_D3;
      cfg.pin_d4 = LGFX_D4;
      cfg.pin_d5 = LGFX_D5;
      cfg.pin_d6 = LGFX_D6;
      cfg.pin_d7 = LGFX_D7;
#ifdef TFT_PARALLEL_16_BIT
      cfg.pin_d8 = LGFX_D8;
      cfg.pin_d9 = LGFX_D9;
      cfg.pin_d10 = LGFX_D10;
      cfg.pin_d11 = LGFX_D11;
      cfg.pin_d12 = LGFX_D12;
      cfg.pin_d13 = LGFX_D13;
      cfg.pin_d14 = LGFX_D14;
      cfg.pin_d15 = LGFX_D15;
#endif

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = LGFX_CS;
      cfg.pin_rst = LGFX_RST;
      cfg.panel_width = TFT_WIDTH;
      cfg.panel_height = TFT_HEIGHT;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false; // 보드의 병렬 인터페이스에서 읽기 기능 지원 여부 문제로 인한 패닉 방지
#ifdef TFT_INVERSION_ON
      cfg.invert = true;
#else
      cfg.invert = false;
#endif

#if defined(TFT_RGB_ORDER) && (TFT_RGB_ORDER == 1)
      cfg.rgb_order = true;
#else
      cfg.rgb_order = false;
#endif
#ifdef TFT_PARALLEL_16_BIT
      cfg.dlen_16bit = true;
#else
      cfg.dlen_16bit = false;
#endif
      cfg.bus_shared = false;

      _panel_instance.config(cfg);
    }
    
    setPanel(&_panel_instance);
  }

};

#endif // USE_LOVYANGFX

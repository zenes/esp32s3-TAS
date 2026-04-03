#pragma once

#ifdef USE_LOVYANGFX

#include <LovyanGFX.hpp>

class LGFX : public lgfx::LGFX_Device
{
  lgfx::Panel_ILI9341 _panel_instance;
  lgfx::Bus_Parallel16 _bus_instance;

public:
  LGFX(void)
  {
    {
      auto cfg = _bus_instance.config();
      cfg.port = 0; // I2S 포트 (ESP32-S3에서는 I2S 또는 LCD_CAM)
      cfg.freq_write = 5000000;
      
      cfg.pin_wr = TFT_WR;
      cfg.pin_rd = TFT_RD;
      cfg.pin_rs = TFT_DC;
      
      cfg.pin_d0 = TFT_D0;
      cfg.pin_d1 = TFT_D1;
      cfg.pin_d2 = TFT_D2;
      cfg.pin_d3 = TFT_D3;
      cfg.pin_d4 = TFT_D4;
      cfg.pin_d5 = TFT_D5;
      cfg.pin_d6 = TFT_D6;
      cfg.pin_d7 = TFT_D7;
      cfg.pin_d8 = TFT_D8;
      cfg.pin_d9 = TFT_D9;
      cfg.pin_d10 = TFT_D10;
      cfg.pin_d11 = TFT_D11;
      cfg.pin_d12 = TFT_D12;
      cfg.pin_d13 = TFT_D13;
      cfg.pin_d14 = TFT_D14;
      cfg.pin_d15 = TFT_D15;

      _bus_instance.config(cfg);
      _panel_instance.setBus(&_bus_instance);
    }

    {
      auto cfg = _panel_instance.config();
      cfg.pin_cs = TFT_CS;
      cfg.pin_rst = TFT_RST;
      cfg.panel_width = TFT_WIDTH;
      cfg.panel_height = TFT_HEIGHT;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.dummy_read_pixel = 8;
      cfg.dummy_read_bits = 1;
      cfg.readable = false;
      cfg.invert = false;
      cfg.rgb_order = false;
      cfg.dlen_16bit = true;
      cfg.bus_shared = false;

      _panel_instance.config(cfg);
    }
    
    setPanel(&_panel_instance);
  }
};

#endif // USE_LOVYANGFX

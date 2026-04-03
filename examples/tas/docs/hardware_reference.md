# ESP32-S3-LCD-EV-BOARD-2 Hardware Specification

이 문서는 ESP32-S3-LCD-EV-BOARD-2의 공식 하드웨어 사양 및 GPIO 할당 정보를 정리한 공식 참조 문서입니다.

## 1. ESP32-S3-WROOM-1 GPIO Allocation

ESP32-S3-WROOM-1 모듈의 핀별 고유 기능 할당 현황입니다.

| Pin | Pin Name | Function | Note |
|:---:|:---|:---|:---|
| 1 | GND | GND | |
| 2 | 3V3 | Power supply | |
| 3 | EN | RESET | |
| 4 | IO4 | LED | |
| 5 | IO5 | I2S_MCLK | |
| 6 | IO6 | I2S_CODEC_DSDIN | |
| 7 | IO7 | I2S_LRCK | |
| 8 | IO15 | I2S_ADC_SDOUT | |
| 9 | IO16 | I2S_SCLK | |
| 10 | IO17 | LCD_DE | |
| 11 | IO18 | LCD_DATA7 | |
| 12 | IO8 | LCD_DATA6 | |
| 13 | IO19 | USB_D- | |
| 14 | IO20 | USB_D+ | |
| 15 | IO3 | LCD_VSYNC | |
| 16 | IO46 | LCD_HSYNC | **Strapping Pin** |
| 17 | IO9 | LCD_PCLK | |
| 18 | IO10 | LCD_DATA0 | |
| 19 | IO11 | LCD_DATA1 | |
| 20 | IO12 | LCD_DATA2 | |
| 21 | IO13 | LCD_DATA3 | |
| 22 | IO14 | LCD_DATA4 | |
| 23 | IO21 | LCD_DATA5 | |
| 24 | IO47 | I2C_SDA | |
| 25 | IO48 | I2C_SCL | |
| 26 | IO45 | LCD_DATA8 | **Strapping Pin** |
| 27 | IO0 | BOOT | **Strapping Pin** |
| 28 | IO35 | No connection | |
| 29 | IO36 | No connection | |
| 30 | IO37 | No connection | |
| 31 | IO38 | LCD_DATA9 | |
| 32 | IO39 | LCD_DATA10 | |
| 33 | IO40 | LCD_DATA11 | |
| 34 | IO41 | LCD_DATA12 | |
| 35 | IO42 | LCD_DATA13 | |
| 36 | RXD0 | UART_RXD0 | |
| 37 | TXD0 | UART_TXD0 | |
| 38 | IO2 | LCD_DATA14 | |
| 39 | IO1 | LCD_DATA15 | |
| 40 | GND | GND | |
| 41 | EPAD | GND | |

## 2. I/O Expander(TCA9554) GPIO Allocation

I2C 버스를 통해 확장된 하드웨어 제어용 핀 할당 현황입니다.

| IO Expander Pin | Pin Name | Function | Note |
|:---:|:---|:---|:---|
| 1 | A0 | GND | |
| 2 | A1 | GND | |
| 3 | A2 | GND | |
| 4 | P0 | PA_CTRL | Audio Power Amp Control |
| 5 | P1 | LCD_SPI_CS | |
| 6 | P2 | LCD_SPI_SCK | |
| 7 | P3 | LCD_SPI_MOSI | |
| 8 | GND | GND | |
| 9 | P4 | Free | |
| 10 | P5 | Free | |
| 11 | P6 | Free | |
| 12 | P7 | Free | |
| 13 | INT | No connection | |
| 14 | SCL | I2C_SCL | |
| 15 | SDA | I2C_SDA | |
| 16 | VCC | Supply voltage | |

---

## 3. 핵심 주의사항 (Strapping & Conflicts)

> [!WARNING]
> **스트래핑 핀 (Strapping Pins)**
> - **GPIO 45**: 부팅 시 Flash 전압(VDD_SPI)을 결정합니다. LCD 데이터 라인으로 인해 전압 레벨이 바뀌면 Flash를 읽지 못해 **무한 리부트**가 발생할 수 있습니다.
> - **GPIO 46**: 부팅 시 이 핀은 강제로 Low 상태여야 정상 부팅이 가능합니다.
> - **GPIO 0**: 부팅 모드를 결정하며, 부팅 시 High 여야 Flash에서 코드가 실행됩니다.

> [!IMPORTANT]
> **오디오 및 LCD 핀 공유 문제**
> - **GPIO 38 ~ 42**: 표에서는 `LCD_DATA9~13`으로 정의되어 있으나, 특정 소프트웨어 환경(`esp32s3box` 등)에서는 이 핀들을 오디오(I2S)용으로 강제 점유하여 충돌을 일으킬 수 있습니다.

#ifndef Pins_Arduino_h
#define Pins_Arduino_h

#include <stdint.h>

// ============================================================
// Custom Board Variant: LilyGO T-ETH-Lite-ESP32S3
// Based on esp32s3box variant (same PSRAM/USB config)
// TFT pins replaced with T-ETH-Lite actual wiring
// ============================================================

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 41;
static const uint8_t SCL = 40;

// Default SPI bus (used by Arduino SPI library)
static const uint8_t SS   = 10;
static const uint8_t MOSI = 11;
static const uint8_t MISO = 13;
static const uint8_t SCK  = 12;

static const uint8_t A8  = 9;
static const uint8_t A9  = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;

static const uint8_t T9  = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;


// TFT 관련 핀들은 platformio.ini의 빌드 플래그(-D)를 
// 온전히 적용받기 위해 이곳에서 선언하지 않습니다.

#endif /* Pins_Arduino_h */

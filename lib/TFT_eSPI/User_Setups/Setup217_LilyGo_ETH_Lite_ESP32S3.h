// ============================================================
// TFT_eSPI Setup File for LilyGO T-ETH-Lite-ESP32S3
// ============================================================
// Board : LilyGO T-ETH-Lite (ESP32-S3 variant)
// ETH   : W5500 (SPI2) on IO9/10/11/12/13/14
// SD    : SPI on IO5/6/7/42
//
// LCD interface is selected at compile time via ONE macro below.
// ⚠ Strapping pins (IO0, IO3, IO45, IO46) must NOT be used.
//   Exception: IO3 is the LilyGO-official LCD MOSI in SPI mode.
// ============================================================

#define USER_SETUP_ID 217

// ============================================================
//  ★ 인터페이스 선택: 아래 둘 중 정확히 하나만 주석 해제 ★
// ============================================================
#define LCD_INTERFACE_SPI            // SPI  — 기본 권장 (SD카드 공존 가능)
// #define LCD_INTERFACE_8BIT_PARALLEL  // 8-bit 병렬 8080 — SD카드 사용 불가
// ============================================================

// 동시 정의 / 미정의 방어
#if defined(LCD_INTERFACE_SPI) && defined(LCD_INTERFACE_8BIT_PARALLEL)
  #error "LCD_INTERFACE_SPI 와 LCD_INTERFACE_8BIT_PARALLEL 를 동시에 정의할 수 없습니다."
#endif
#if !defined(LCD_INTERFACE_SPI) && !defined(LCD_INTERFACE_8BIT_PARALLEL)
  #error "LCD_INTERFACE_SPI 또는 LCD_INTERFACE_8BIT_PARALLEL 중 하나를 반드시 정의하세요."
#endif

// ============================================================
//  드라이버 IC 선택 (실제 패널에 맞게 하나만 활성화)
// ============================================================
#if defined(LCD_INTERFACE_SPI)
  // SPI 지원 드라이버 (하나만 활성화)
  // #define ILI9341_DRIVER     // 240x320 — 가장 일반적
   #define ST7789_DRIVER   // 240x240 / 170x320 등
  // #define GC9A01_DRIVER   // 240x240 원형

#elif defined(LCD_INTERFACE_8BIT_PARALLEL)
  // 8-bit 병렬 지원 드라이버 (하나만 활성화)
  #define TFT_PARALLEL_8_BIT
  #define ILI9341_DRIVER     // 240x320
  // #define ILI9488_DRIVER  // 480x320 (SPI 18-bit와 달리 8-bit 병렬로 고속 가능)
#endif

// ============================================================
//  SPI 인터페이스 핀
//  (utilities.h의 LCD_xxx_PIN 정의와 일치)
//  IO3(MOSI): 스트래핑 핀이나 LilyGO 공식 LCD SPI 핀으로 지정됨
// ============================================================
#if defined(LCD_INTERFACE_SPI)
  #define TFT_MISO   1   // P3 헤더 IO1  — LCD_MISO_PIN
  #define TFT_MOSI   3   // P3 헤더 IO3  — LCD_MOSI_PIN (스트래핑 핀, LilyGO 공식)
  #define TFT_SCLK   2   // P3 헤더 IO2  — LCD_SCLK_PIN
  #define TFT_CS     4   // P2 헤더 IO4  — LCD_CS_PIN
  #define TFT_DC     8   // P2 헤더 IO8  — LCD_DC_PIN
  #define TFT_RST   15   // P2 헤더 IO15 — LCD_RST_PIN
  #define TFT_BL    16   // P2 헤더 IO16 — LCD_BL_PIN
  #define TFT_BACKLIGHT_ON HIGH

// ============================================================
//  8-bit 병렬(Intel 8080) 인터페이스 핀
//  제외 기준:
//    - 스트래핑 핀 : IO0, IO3, IO45, IO46
//    - ETH W5500   : IO9, IO10, IO11, IO12, IO13, IO14
//    - SD 카드     : IO5, IO6, IO7, IO42
//    - USB 내부    : IO19, IO20
//  TFT_eSPI 요건: 데이터 버스(D0~D7)는 GPIO 0-31 범위 내
// ============================================================
#elif defined(LCD_INTERFACE_8BIT_PARALLEL)
  #define TFT_CS    21   // P3 헤더 IO21 — CS  (단일 디스플레이 시 GND 직결 가능)
  #define TFT_DC    38   // P3 헤더 IO38 — DC/RS (명령 vs 데이터 구분 신호)
  #define TFT_RST   39   // P3 헤더 IO39 — RST
  #define TFT_WR    40   // P3 헤더 IO40 — WR (데이터 래치 클럭)
  #define TFT_RD    41   // P3 헤더 IO41 — RD  (읽기 미사용 시 3.3V 직결 가능)

  // 데이터 버스 D0~D7 (모두 GPIO 0-31 범위 내)
  #define TFT_D0     1   // P3 헤더 IO1
  #define TFT_D1     2   // P3 헤더 IO2
  #define TFT_D2     4   // P2 헤더 IO4
  #define TFT_D3     8   // P2 헤더 IO8
  #define TFT_D4    15   // P2 헤더 IO15
  #define TFT_D5    16   // P2 헤더 IO16
  #define TFT_D6    17   // P2 헤더 IO17
  #define TFT_D7    18   // P2 헤더 IO18

  #define TFT_BL    47   // P4 헤더 IO47 — 백라이트
  #define TFT_BACKLIGHT_ON HIGH
#endif

// ============================================================
//  공통 폰트 설정
// ============================================================
#define LOAD_GLCD   // Font 1 — Adafruit 8px 기본 폰트 (~1820 bytes)
#define LOAD_FONT2  // Font 2 — 16px 소형 폰트  (~3534 bytes)
#define LOAD_FONT4  // Font 4 — 26px 중형 폰트  (~5848 bytes)
#define LOAD_FONT6  // Font 6 — 48px 숫자 전용  (~2666 bytes)
#define LOAD_FONT7  // Font 7 — 48px 7-Segment  (~2438 bytes)
#define LOAD_FONT8  // Font 8 — 75px 대형 폰트  (~3256 bytes)
#define LOAD_GFXFF  // Adafruit GFX Free Fonts (FF1~FF48)
#define SMOOTH_FONT // 스무스 폰트 (SPIFFS 사용 시 활성화)

// ============================================================
//  SPI 클럭 설정 (Software SPI 모드에서는 무시될 수 있음)
// ============================================================
#if defined(LCD_INTERFACE_SPI)
  // #define SOFT_SPI            // 하드웨어 모드로 복구
  // #define SPI_FREQUENCY       20000000  // 20MHz - 하드웨어 최적 속도
  #define SPI_READ_FREQUENCY  5000000
#endif

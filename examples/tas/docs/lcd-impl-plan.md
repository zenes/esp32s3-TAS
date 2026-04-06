# LCD 인터페이스 전처리문 기반 구현 계획

## 개요

`lcd.md` 분석을 기반으로, LilyGO T-ETH-Lite-ESP32S3 보드에서 **SPI**와 **8-bit 병렬(Intel 8080)** LCD 인터페이스를 모두 지원하는 TFT_eSPI 설정 파일을 전처리문(`#ifdef`)으로 분기 처리합니다.

> **[NOTE]** T-ETH-Lite 보드 기준으로는 16-bit 인터페이스가 불가능하지만, **ESP32-S3-LCD-EV-BOARD-2 (EVB)** 환경에서는 공식 핀맵을 통해 **16-bit 병렬(MCU 8080)** 구동이 가능합니다. 이 프로젝트는 두 보드 사양을 모두 지원하도록 설계되었습니다.

---

## 현황 분석

| 항목 | 현재 상태 |
|---|---|
| 활성 Setup 파일 | `User_Setup_Select.h` → `Setup216_LilyGo_ETH_Pro_ESP32.h` (T-ETH-Pro용, 핀 불일치) |
| 보드 정의 | `utilities.h`에 `LILYGO_T_ETH_LITE_ESP32S3` 활성화 |
| LCD SPI 핀 | `utilities.h`에 이미 정의됨 (`MISO=IO1, MOSI=IO3, SCLK=IO2, CS=IO4, DC=IO8, RST=IO15, BL=IO16`) |
| 8-bit 병렬 핀 | 미정의 — 아래 안전 핀 목록에서 선택 |

---

## ESP32-S3 스트래핑 핀 (절대 사용 금지)

> **[CAUTION]** 아래 4개 핀은 **부팅 모드 결정에 관여**하므로, SPI/병렬 어느 인터페이스에서도 사용하면 안 됩니다.

| 핀 | 역할 |
|---|---|
| IO0 | 부트 모드 선택 (LOW = Download mode) |
| IO3 | JTAG 소스 선택 |
| IO45 | VDD_SPI 전압 선택 (3.3V / 1.8V) |
| IO46 | ROM 시리얼 출력 활성여부 |

> **[WARNING]** `utilities.h`의 SPI 모드 `LCD_MOSI_PIN = IO3`은 LilyGO 공식 핀 배정이므로 유지합니다. IO3는 부팅 후 GPIO로 사용 가능하나, 외부 풀업/풀다운 회로가 없는 경우에만 안전합니다.

---

## 안전 핀 목록 (8-bit 병렬 후보)

ETH(IO9~14), SD(IO5~7), 스트래핑(IO0,IO3,IO45,IO46), USB내부(IO19,IO20) 제외 후 사용 가능한 핀:

| 헤더 | 안전 GPIO |
|---|---|
| P3 | IO1, IO2, IO21, IO38, IO39, IO40, IO41 |
| P2 | IO4, IO8, IO15, IO16, IO17, IO18 |
| P4 | IO47, IO48 |
| SD 슬롯 위치 | IO5, IO6, IO7 (SD 미사용 시) |

---

## 제안 변경 사항

### 1. TFT_eSPI User_Setup 파일 신규 생성

**[NEW]** `lib/TFT_eSPI/User_Setups/Setup217_LilyGo_ETH_Lite_ESP32S3.h`

```cpp
#define USER_SETUP_ID 217

// ============================================================
// 인터페이스 선택: 아래 둘 중 정확히 하나만 활성화
// ============================================================
#define LCD_INTERFACE_SPI            // SPI (기본 권장, SD카드 공존 가능)
// #define LCD_INTERFACE_8BIT_PARALLEL  // 8-bit 병렬 (SD카드 사용 불가)
// ============================================================

// 두 매크로 동시 정의 방지
#if defined(LCD_INTERFACE_SPI) && defined(LCD_INTERFACE_8BIT_PARALLEL)
  #error "LCD_INTERFACE_SPI와 LCD_INTERFACE_8BIT_PARALLEL 중 하나만 정의하세요."
#endif
#if !defined(LCD_INTERFACE_SPI) && !defined(LCD_INTERFACE_8BIT_PARALLEL)
  #error "LCD_INTERFACE_SPI 또는 LCD_INTERFACE_8BIT_PARALLEL 중 하나를 반드시 정의하세요."
#endif

// 드라이버 IC 선택 (실제 패널에 맞게 선택)
#if defined(LCD_INTERFACE_SPI)
  #define ILI9341_DRIVER   // SPI: ILI9341(240x320), ST7789, GC9A01 등
  // #define ST7789_DRIVER
#elif defined(LCD_INTERFACE_8BIT_PARALLEL)
  #define TFT_PARALLEL_8_BIT
  #define ILI9341_DRIVER   // 8-bit 병렬: ILI9341, ILI9488 등
  // #define ILI9488_DRIVER
#endif

// ── SPI 인터페이스 핀 ──────────────────────────────────────
// utilities.h의 LCD_xxx 핀과 일치 (LilyGO 공식 핀 배정)
// IO3(MOSI)은 스트래핑 핀이나 LilyGO 설계상 사용 — 외부 풀업/풀다운 없이 사용
#if defined(LCD_INTERFACE_SPI)
  #define TFT_MISO   1   // LCD_MISO_PIN (P3)
  #define TFT_MOSI   3   // LCD_MOSI_PIN (P3) ← 스트래핑 핀 IO3, LilyGO 공식 지정
  #define TFT_SCLK   2   // LCD_SCLK_PIN (P3)
  #define TFT_CS     4   // LCD_CS_PIN   (P2)
  #define TFT_DC     8   // LCD_DC_PIN   (P2)
  #define TFT_RST   15   // LCD_RST_PIN  (P2)
  #define TFT_BL    16   // LCD_BL_PIN   (P2)
  #define TFT_BACKLIGHT_ON HIGH

// ── 8-bit 병렬(8080) 인터페이스 핀 ───────────────────────
// 스트래핑 핀(IO0,IO3,IO45,IO46), ETH(IO9~14), SD(IO5~7) 모두 제외
// 데이터 버스는 TFT_eSPI 요건상 GPIO 0-31 범위 내이어야 함
#elif defined(LCD_INTERFACE_8BIT_PARALLEL)
  #define TFT_CS    21   // P3: IO21 — CS (단일 디스플레이 시 GND 직결 가능)
  #define TFT_DC    38   // P3: IO38 — DC/RS (Command vs Data 구분)
  #define TFT_RST   39   // P3: IO39 — RST
  #define TFT_WR    40   // P3: IO40 — WR 클럭 (데이터 래치 트리거)
  #define TFT_RD    41   // P3: IO41 — RD (읽기 불필요 시 3.3V 직결 가능)
  // 데이터 버스 D0-D7 (TFT_eSPI: GPIO 0-31 범위 필수)
  #define TFT_D0     1   // P3: IO1
  #define TFT_D1     2   // P3: IO2
  #define TFT_D2     4   // P2: IO4
  #define TFT_D3     8   // P2: IO8
  #define TFT_D4    15   // P2: IO15
  #define TFT_D5    16   // P2: IO16
  #define TFT_D6    17   // P2: IO17
  #define TFT_D7    18   // P2: IO18
  #define TFT_BL    47   // P4: IO47 — 백라이트
  #define TFT_BACKLIGHT_ON HIGH

// ── 16-bit 병렬(8080) 인터페이스 핀 (evb-lcd3 전용) ─────────
// 이 설정은 ESP32-S3-LCD-EV-BOARD-2의 공식 핀맵을 따릅니다.
// T-ETH-Lite 보드에서는 이더넷 SPI3 및 스트래핑 핀과 충돌하므로 사용 주의.
#elif defined(LCD_INTERFACE_16BIT_PARALLEL)
  #define TFT_WR    17
  #define TFT_RD     4
  #define TFT_DC     9   // Command/Data
  #define TFT_CS    46   // Strapping Pin!
  #define TFT_RST    3   // Strapping Pin!
  #define TFT_D0    10
  #define TFT_D1    11
  #define TFT_D2    12
  #define TFT_D3    13
  #define TFT_D4    14
  #define TFT_D5    21
  #define TFT_D6     8
  #define TFT_D7    18
  #define TFT_D8    45   // Strapping Pin!
  #define TFT_D9    38
  #define TFT_D10   39
  #define TFT_D11   40
  #define TFT_D12   41
  #define TFT_D13   42
  #define TFT_D14    2
  #define TFT_D15    1
  #define TFT_BL    -1   // Not used
#endif

// 공통 폰트 설정
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

// SPI 클럭 (SPI 모드에서만 유효)
#if defined(LCD_INTERFACE_SPI)
  #define SPI_FREQUENCY       40000000  // 40MHz (ILI9341 안정 동작)
  #define SPI_READ_FREQUENCY  20000000
  #define SPI_TOUCH_FREQUENCY  2500000
#endif
```

---

### 2. User_Setup_Select.h 수정

**[MODIFY]** `lib/TFT_eSPI/User_Setup_Select.h`

```diff
- #include <User_Setups/Setup216_LilyGo_ETH_Pro_ESP32.h>
+ // #include <User_Setups/Setup216_LilyGo_ETH_Pro_ESP32.h>   // T-ETH-Pro 전용
+ #include <User_Setups/Setup217_LilyGo_ETH_Lite_ESP32S3.h>  // T-ETH-Lite-ESP32S3
```

---

### 3. utilities.h 확장 (선택 사항)

**[MODIFY]** `examples/tas/utilities.h` — `LILYGO_T_ETH_LITE_ESP32S3` 섹션에 추가

```c
// [추가] 8-bit 병렬 LCD 핀 (SD카드 미사용 조건, 스트래핑 핀 제외)
#define LCD8_CS_PIN   21
#define LCD8_DC_PIN   38
#define LCD8_RST_PIN  39
#define LCD8_WR_PIN   40
#define LCD8_RD_PIN   41
#define LCD8_D0_PIN    1
#define LCD8_D1_PIN    2
#define LCD8_D2_PIN    4
#define LCD8_D3_PIN    8
#define LCD8_D4_PIN   15
#define LCD8_D5_PIN   16
#define LCD8_D6_PIN   17
#define LCD8_D7_PIN   18
#define LCD8_BL_PIN   47
```

> **[WARNING]** 8-bit 병렬 사용 시 SD카드 사용 불가 — `LCD8_D0(IO1)`, `LCD8_D2(IO4)`가 SPI LCD 핀과 겹치므로, 병렬 모드 선택 시 SD 초기화 코드를 반드시 비활성화해야 합니다.

> **[IMPORTANT]** 스케치 코드(TFT 초기화)는 인터페이스와 무관하게 동일 — `TFT_eSPI tft; tft.init();`은 변경 없이 사용 가능. 전처리 분기는 Setup 파일 내에서 완결됩니다.

---

## 핀 사용 요약표

| GPIO | SPI 모드 역할 | 8-bit 병렬 역할 | 스트래핑? | 비고 |
|:---:|---|---|:---:|---|
| IO1 | TFT_MISO | TFT_D0 | — | 모드별 독점 |
| IO2 | TFT_SCLK | TFT_D1 | — | 모드별 독점 |
| **IO3** | **TFT_MOSI** | — | **⚠️ YES** | LilyGO 공식 지정, 병렬 미사용 |
| IO4 | TFT_CS | TFT_D2 | — | 모드별 독점 |
| IO8 | TFT_DC | TFT_D3 | — | 모드별 독점 |
| IO15 | TFT_RST | TFT_D4 | — | 모드별 독점 |
| IO16 | TFT_BL | TFT_D5 | — | 모드별 독점 |
| IO17 | — | TFT_D6 | — | 병렬 전용 |
| IO18 | — | TFT_D7 | — | 병렬 전용 |
| IO21 | — | TFT_CS | — | 병렬 전용 |
| IO38 | — | TFT_DC | — | 병렬 전용 |
| IO39 | — | TFT_RST | — | 병렬 전용 |
| IO40 | — | TFT_WR | — | 병렬 전용 |
| IO41 | — | TFT_RD | — | 병렬 전용 |
| IO47 | — | TFT_BL | — | 병렬 전용 |

두 모드는 핀 상당 부분이 겹쳐 **동시 연결 불가** — `LCD_INTERFACE_SPI` / `LCD_INTERFACE_8BIT_PARALLEL` 중 하나만 활성화.

---

## Verification Plan

### 빌드 검증 (컴파일 타임)
```bash
# PlatformIO
pio run -e lilygo-t-eth-lite-s3 -t clean && pio run -e lilygo-t-eth-lite-s3
# Arduino IDE: Sketch → Verify/Compile
```

### 수동 검증 절차

1. **SPI 모드**: `#define LCD_INTERFACE_SPI` 활성화 → LCD를 SPI 핀에 연결 → 업로드 → 화면 출력 + 이더넷/SD 동작 확인
2. **8-bit 병렬 모드**: `#define LCD_INTERFACE_8BIT_PARALLEL` 활성화 → SD 코드 비활성화 → 병렬 핀에 연결 → 업로드 → 화면 출력 + FPS 비교
3. **방어 코드 검증**: 두 매크로 동시 정의 시 `#error` 컴파일 오류 발생 확인

# 🚀 TAS 프로젝트 전체 빌드, 하드웨어/소프트웨어 튜닝 가이드

### 1. 🏗️ ESP-IDF 타겟별 빌드 가이드 (ENV 프로필)

TAS 프로젝트는 `CMakeLists.txt`에 사전 정의된 환경 변수(`ENV`)를 통해 디스플레이 종류와 통신 방식(SPI, 8비트, 16비트 병렬)을 자유롭게 전환합니다.

*   **환경 준비**: 터미널에서 ESP-IDF `export.sh`를 소싱합니다.
*   **플래싱 및 모니터링 공통 명령어**:
    ```bash
    idf.py -p [장치포트] flash monitor
    ```

#### 📌 타겟별 빌드 명령어 모음
1.  **lilygo-lcd1-p8 (현재 주력 모드)**: `idf.py build -DENV=lilygo-lcd1-p8` (8비트 병렬, ST7789, LovyanGFX)
2.  **lilygo-lcd1**: `idf.py build -DENV=lilygo-lcd1` (기본 SPI, ST7789)
3.  **lilygo-lcd2**: `idf.py build -DENV=lilygo-lcd2` (기본 SPI, ILI9341)
4.  **lilygo-lcd3**: `idf.py build -DENV=lilygo-lcd3` (16비트 병렬, ILI9341)
5.  **evb-lcd3**: `idf.py build -DENV=evb-lcd3` (ESP32-S3 EVB 보드 16비트 병렬, ILI9341)

---

### 2. 📝 CMakeLists.txt 파일 수정법 (라이브러리 및 설정 변경)

새로운 보드를 추가하거나 LCD 라이브러리를 변경하려면 `main/CMakeLists.txt`의 `ENV` 조건문을 수정해야 합니다.

*   **LCD 라이브러리 지정법 (`USE_LOVYANGFX`)**:
    프로필 내에 `USE_LOVYANGFX=1` 매크로를 추가하면 LovyanGFX가 선택되며, 생략하면 기본 TFT_eSPI가 선택됩니다.
*   **커스텀 프로필 추가 및 매크로(FPS 등) 설정 예시**:
    `main/CMakeLists.txt` 파일에 아래와 같이 `EXTRA_DEFS`를 추가합니다.
    ```cmake
    elseif("${ENV}" STREQUAL "my-custom-board")
        # 1. 보드 및 패널 정의, LovyanGFX 활성화
        list(APPEND EXTRA_DEFS LILYGO_T_ETH_LITE_ESP32S3=1 LCD_TYPE_ST7789=1 ENABLE_LCD=1 USE_LOVYANGFX=1)
        
        # 2. LovyanGFX 병렬 핀 매핑 (LGFX_ 접두사 사용)
        list(APPEND EXTRA_DEFS LGFX_WR=40 LGFX_RD=41 LGFX_DC=38 LGFX_CS=21 LGFX_RST=39)
        list(APPEND EXTRA_DEFS LGFX_D0=1 LGFX_D1=2 LGFX_D2=4 LGFX_D3=8 LGFX_D4=15 LGFX_D5=16 LGFX_D6=17 LGFX_D7=18)
        
        # 3. LVGL 타겟 주사율(FPS) 등 전역 설정 강제 주입 (33ms = 30FPS)
        list(APPEND EXTRA_DEFS LV_DISP_DEF_REFR_PERIOD=33)
    ```

---

### 3. ⚙️ sdkconfig 설정 및 영구 저장 가이드 (sdkconfig.defaults)

ESP-IDF의 커널 설정(코어 분리, TCP 윈도우 사이즈 등)을 변경하고 이를 프로젝트에 영구적으로 반영하는 방법입니다.

#### A. 일시적 설정 변경 (테스트용)
터미널에서 메뉴 기반으로 설정을 변경할 수 있습니다. (빌드 폴더 내 `sdkconfig` 파일에 임시 저장됨)
```bash
idf.py menuconfig
```

#### B. 설정 영구 저장법 (`sdkconfig.defaults`)
`menuconfig`로 설정한 값은 `idf.py fullclean` 시 날아갈 수 있습니다. 이를 방지하려면 프로젝트 루트에 있는 `sdkconfig.defaults` 파일을 직접 수정해야 합니다. 빌드 시 이 파일의 내용이 최우선으로 반영됩니다.

*   **`sdkconfig.defaults` 권장 작성 예시**:
    ```ini
    # 1. Network Core Affinity (Core 0 전담)
    CONFIG_LWIP_TCPIP_TASK_AFFINITY=0x0
    CONFIG_ESP32_WIFI_TASK_PINNED_TO_CORE_0=y

    # 2. App/UI Core Affinity (Core 1 전담)
    CONFIG_ARDUINO_RUNNING_CORE=1
    CONFIG_ARDUINO_EVENT_RUNNING_CORE=1

    # 3. 고속 통신을 위한 TCP 윈도우 사이즈 영구 상향
    CONFIG_LWIP_TCP_WND_DEFAULT=65535
    CONFIG_LWIP_TCP_SND_BUF_DEFAULT=65535

    # 4. WiFi 절전 모드 차단 (레이턴시 최적화)
    CONFIG_ESP32_WIFI_IRAM_OPT=y
    ```
*   **주의**: `sdkconfig.defaults`를 수정한 후에는 반드시 기존 캐시를 날리고 다시 빌드해야 적용됩니다.
    ```bash
    rm -rf build/sdkconfig build/CMakeCache.txt
    idf.py build -DENV=lilygo-lcd1-p8
    ```

---

### 4. ⏱️ 클럭(Clock) 속도 튜닝 방법

*   **LCD WR (Write) Clock 조정 (LovyanGFX)**:
    *   **위치**: `main/LGFX_Config.hpp` 내 `LGFX()` 생성자
    *   `cfg.freq_write = 20000000;` (현재 20MHz). 안정성이 확보되면 40MHz로 상향하여 화면 갱신 속도를 끌어올릴 수 있습니다.
*   **이더넷(W5500) SPI 클럭 조정**:
    *   **위치**: `main/utilities.h` 의 `#define W5500_SPI_CLOCK_MHZ 40`
    *   기본 40MHz. 핑 로스나 연결 끊김이 발생하면 20MHz로 하향합니다.

---

### 5. ⚖️ 태스크 우선순위 (Task Priority) 튜닝 가이드

네트워크 대역폭과 UI 응답성 사이의 소프트웨어적 밸런스를 조절합니다. (숫자가 높을수록 우선순위 상승)

*   **튜닝 방법**:
    `main/utilities.h`에서 `#define ENABLE_TASK_PRIORITY_TUNING` 활성화 시, TCP/IP(`tiT`) 태스크의 우선순위가 24로 상향됩니다.
*   **기아(Starvation) 현상 방지**:
    고속 통신(iPerf 등)이 UI 태스크를 멈추게 한다면, `toe_iperf.cpp` 내부의 통신 무한 루프 구간처럼 강제로 CPU를 양보하는 코드(`vTaskDelay(1)`)를 20ms 간격으로 삽입하여 해결합니다.

---

### 6. 🛠️ 디버깅 및 시각적 테스트용 SW 설정 (`utilities.h`)

기능을 켜고 끄며 성능 저하의 원인을 격리할 수 있는 매크로 스위치들입니다.

*   `#define ENABLE_ETHERNET`: 주석 처리 시 네트워크 드라이버 로드를 차단하고 **순수 LCD 렌더링 성능만 격리하여 측정**합니다.
*   `#define ENABLE_TEST_BALL`: 화면에 움직이는 노란 공을 띄워, 네트워크 부하 시 **화면 찢어짐(Tearing)이나 프레임 드랍(버벅임)을 육안으로 확인**합니다.
*   `#define ENABLE_DELTATIME_ANIM`: 프레임 속도가 저하되더라도 애니메이션의 실제 이동 속도를 시간에 비례하여 일정하게 유지시켜 줍니다.
*   `#define ENABLE_GRADIENT_BG`: 그라데이션 부하를 켭니다. 주석 처리하여 단색 배경으로 만들면 CPU 점유율이 어떻게 변하는지 비교할 수 있습니다.

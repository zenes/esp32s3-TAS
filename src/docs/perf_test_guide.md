# LCD 성능 직접 테스트 가이드 (Performance Test Guide)

사용자께서 직접 성능을 측정하고 기록할 수 있는 테스트 설정 시트와 가이드입니다. 

### 📝 테스트 결과 기록 테이블

| 테스트 회차 | SPI 클럭 | 버퍼 라인 수 | 비동기 여부 | 예상 결과 | 실제 FPS | 실제 CPU (%) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Test 1** | 40MHz | 64 | Async | ~31 FPS | | |
| **Test 2** | 80MHz | 160 | Async | ~39 FPS | | |
| **Test 3** | 80MHz | 80 | Async | ~52 FPS | | |
| **Test 4** | 80MHz | 64 | Async | **53 FPS** | | |
| **Test 5** | 80MHz | 64 | **Blocking** | ~29 FPS | | |

---

### 🛠️ 설정 변경 방법 (Factor별 수정 위치)

#### 1. SPI 클럭 (SPI Frequency)
*   **파일**: `platformio.ini`
*   **수정 코드**: `-DSPI_FREQUENCY=80000000` (또는 `40000000`)
*   **설명**: 하드웨어 전송 대역폭의 한계를 결정합니다.

#### 2. 버퍼 라인 수 (Buffer Lines)
*   **파일**: `examples/eth2ap/eth2ap.ino` (약 1377라인)
*   **수정 코드**: `size_t lines = 64;` (80 또는 160으로 변경 가능)
*   **설명**: 64라인은 ESP32-S3의 L1 캐시(32KB)에 딱 맞으며, 160라인은 캐시 범위를 초과합니다.

#### 3. 비동기/블로킹 모드 (Sync Mode)
*   **파일**: `examples/eth2ap/eth2ap.ino` (약 1358라인, `my_disp_flush` 함수)
*   **비동기 (Async)**: `tft.endWrite();` 주석 처리 상태
*   **블로킹 (Blocking)**: `tft.endWrite();` 주석 해제 상태
*   **설명**: 전송이 끝날 때까지 기다릴지(`Blocking`), 아니면 즉시 다음 작업을 수행할지(`Async`)를 결정합니다.

#### 4. 주사율 제한 (Refresh Period)
*   **파일**: `include/lv_conf.h` (약 39라인)
*   **수정 코드**: `#define LV_DISP_DEF_REFR_PERIOD 16`
*   **설명**: 16ms로 설정하면 60 FPS가 타겟이며, 33ms로 설정하면 30 FPS가 타겟입니다.

### 📊 결과 확인 방법
1.  코드를 수정하고 **업로드**합니다.
2.  LCD 화면 오른쪽 하단에 실시간으로 표시되는 **FPS: XX** 수치를 확인합니다.
3.  시리얼 모니터를 통해 출력되는 로그에서 **CPU 사용량**이나 전송 상태를 관찰합니다.

직접 테스트해보시면서 사용자님 환경에 가장 최적화된 "황금 수치"를 찾아보시기 바랍니다!

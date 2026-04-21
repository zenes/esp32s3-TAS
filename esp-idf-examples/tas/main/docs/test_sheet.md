# ESP32-S3 LCD 성능 테스트 시트 (ST7789, 240x320)

주요 성능 팩터인 SPI 클럭, 동기화 방식, 버퍼 크기에 따른 성능 데이터를 정리한 공식 테스트 시트입니다.

### 📊 종합 성능 매트릭스

| 테스트 케이스 | SPI CLOCK | 동기화 방식 | 버퍼 크기 (라인 수) | CPU 점유율 (%) | 프레임 레이트 (FPS) | 비고 (주요 원인) |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Baseline** | 40MHz | Blocking | Single (20) | ~100% | **18 FPS** | 전송 대역폭 & 블로킹 병목 |
| **Low Bandwidth** | 40MHz | **Async (DMA)** | **Double (64)** | ~100% | **31 FPS** | **전송 전용 대역폭 병목** (Busy-wait) |
| **Mid-Step 1** | 80MHz | Blocking | Double (160) | ~86% | **28~29 FPS** | 클럭은 높으나 대기 오버헤드로 인한 30 FPS 미달 |
| **Mid-Step 2** | 80MHz | **Async (DMA)** | Double (160) | ~84% | **31 FPS** | **33ms(30 FPS) 소프트웨어 캡**에 걸림 |
| **Cache Thrash** | 80MHz | **Async (DMA)** | **Double (160)** | ~90% | **39 FPS** | 16ms 캡 해제했으나 **캐시 부족(75KB)**으로 속도 하락 |
| **Sweet Spot 1** | 80MHz | **Async (DMA)** | **Double (80)** | **~82%** | **52 FPS** | 캐시 효율 개선 (37.5KB) |
| **Final Peak** | 80MHz | **Async (DMA)** | **Double (64)** | **~82%** | **53 FPS** | **L1 캐시(32KB) 1:1 매칭.** 이론적 최강 성능 도달 |

---

### 🔍 주요 Factor 분석
1.  **SPI Clock (대역폭)**: 40MHz에서 80MHz로 상향 시, 물리적으로 보낼 수 있는 FPS의 천장이 약 30 FPS에서 60 FPS 수준으로 확장됩니다.
2.  **동기화 방식 (병렬화)**: `tft.endWrite()`와 같은 블로킹 호출을 제거하는 것만으로도 CPU 연산과 전송이 병렬로 처리되어 약 10~15ms의 추가 시간을 벌 수 있습니다.
3.  **버퍼 크기 (캐시 효율)**: ESP32-S3의 L1 Data Cache(32KB)보다 버퍼가 크면 CPU가 픽셀을 채워 넣는 속도가 현저히 느려집니다. **64라인(30.7KB)**은 이 캐시에 완벽히 들어가는 "마법의 숫자"입니다.
4.  **REFR_PERIOD (소프트웨어 지연)**: 하드웨어가 아무리 빨라도 라이브러리 내부의 주사율 제한이 33ms(30 FPS)이면 그 이상을 기록할 수 없습니다.

### 📋 권장 최종 설정 값
*   **SPI Frequency**: `80,000,000` (80 MHz)
*   **Buffering Strategy**: Double Buffering with Asynchronous DMA (no `endWrite` in `flush_cb`)
*   **Buffer Lines**: `64` (Standard Landscape / 240 pixels width)
*   **LV_DISP_DEF_REFR_PERIOD**: `16` (60 FPS Target)

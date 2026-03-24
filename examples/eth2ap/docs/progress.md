# 프로젝트 진행 상황

## 2026-03-20
- **LCD 디버깅을 위한 이더넷 격리**:
    - `utilities.h`에 `ENABLE_ETHERNET` 토글 추가 (충돌 방지를 위해 기본적으로 비활성화).
    - `eth2ap.ino` 내의 이더넷 초기화, 이벤트 핸들러, 루프 로직을 `#ifdef ENABLE_ETHERNET`으로 감싸 처리.
    - `eth2ap.ino` 내의 이더넷 관련 셸 명령어(`stats`, `iperf`, `dhcp` 등)를 격리.
- **빌드 및 호환성 수정**:
    - `platformio.ini`의 소스 필터를 수정하여 모든 소스 파일(`+<*>`)이 포함되도록 수정.
    - `lib_ignore`에서 누락되었던 필수 라이브러리(`ETHClass2`, `ESP32Ping`, `lvgl`) 복구.
    - `eth2ap.ino` 내의 Arduino Core 2.0.x 호환성 문제 해결 (`WiFi.AP.netif()` 관련).
    - LCD 타이틀 폰트를 `lv_font_montserrat_14`로 변경하여 링크 에러 해결.
- **문서화 및 워크플로우**:
    - 상세한 SPI 채널 분석 및 설정 내용을 담은 `docs/spi.md` 생성.
    - LCD 구현 진행 상황을 추적하기 위해 `docs/todo.md` 업데이트.
    - 자동화된 문서화 및 릴리스를 위해 `/commit` 워크플로우를 등록 및 실행.
    - 개발 시작 전 문맥 파악을 위한 `/load` 워크플로우 등록 및 `load.md` 작성.
- **워크플로우 가이드라인 강화**:
    - 모든 문서 업데이트 시 **한글**- [x] 프로젝트 변경 사항 커밋 `@[/commit]`
    - [x] 변경 사항 분석 (`git status`, `git diff`)
    - [x] `progress.md` 업데이트 (한글)
    - [x] 스테이징 및 커밋 (`git add`, `git commit`)
    - [x] 원격 저장소 푸시 (`git push`)
- [x] FPS 성능 개선 (SRAM DMA 더블 버퍼링 적용)
    - [x] SRAM/PSRAM 싱글 버퍼 테스트 완료
    - [x] 80MHz SPI 클럭 설정 완료 (25 FPS 확인)
    - [x] SRAM DMA 더블 버퍼(160라인 x 2) 코드 작성 및 업로드
    - [x] 결과 확인 (30 FPS 도달 기대)
- `utilities.h`에 `ENABLE_LCD` 매크로 정의 및 관련 기능 활성화.
- **하드웨어 디버깅 계획 수립**:
    - `todo.md`에 오실로스코프를 이용한 SPI 신호(SCLK, MOSI, MISO, RST, CS) 정밀 분석 계획 반영.

## 2026-03-23
- **LCD SPI 클럭 및 하드웨어 감지 이슈 해결**:
    - **SPI 버스 충돌 해결**: 기본 SPI 객체가 이더넷 모듈과 동일한 SPI3_HOST를 공유하여 발생하는 충돌을 확인하고, LCD용 버스를 **SPI2_HOST (FSPI)**로 명시적 분리하여 클럭 신호 누락 문제 해결.
    - **더미 바이트 보정**: ST7789V 데이터 시트 분석을 통해 `0x04`, `0xDA/B/C` 명령어 사용 시 필수적인 **1바이트 더미 읽기(Dummy read)** 로직 적용.
    #### 1. 주요 작업 내용
*   **LCD 성능 최적화 실험**:
    *   **SPI 클럭**: 40MHz -> 80MHz 상향 성공.
    *   **버퍼 전략 최적화**:
        *   **SRAM Single (20~240 line)**: 최대 20 FPS (Blocking).
        *   **PSRAM Single (Full)**: 80MHz 시 최대 25-26 FPS (Blocking).
        *   **SRAM DMA Double (160 line x 2)**: **최종 30 FPS 달성 목표 적용 완료**.
    *   **메모리 최적화**: `MALLOC_CAP_DMA`를 통한 내부 SRAM 효율적 활용 (81% 사용).
- **하드웨어 감지 성공**: `RDID2/3` 명령어를 통해 유효한 디지털 패턴(`F0`, `0F`)을 검출하여 LCD 하드웨어 존재 증명 및 화면 출력 성공 확인.
- **로우레벨 디버깅 및 분석**:
    - GPIO Matrix 및 IO_MUX 레지스터 덤프 기능을 구현하여 핀 매핑 및 내부 시그널(101 vs 63) 충돌 원인 규명.
    - 비트뱅잉(Bit-banging) 테스트를 통해 더미 비트 가설 검증 및 칩 특성 분석.
- **LCD 색상 및 성능 검증 및 최적화**:
    - 실시간 FPS 측정을 위한 카운터 및 UI 라벨 추가.
    - 색상 정확도 검증을 위한 R, G, B, W, K 테스트 팔레트 구현.
    - **색상 및 텍스트 왜곡 해결**:
        - `platformio.ini`에 `TFT_RGB_ORDER=TFT_BGR` 추가 (Red-Blue 스왑 해결).
        - `LV_COLOR_16_SWAP=1` 및 `pushColors(..., false)` 설정을 통해 엔디안(Endian) mismatch로 인한 글자 깨짐 및 3방향 색상 밀림 현상 완전 해결.
- **문서화 및 가이드라인**:
    - LCD 구동 분석 및 최적 설정 가이드를 정리한 `docs/lcd.md` 생성 및 `docs` 폴더로 이동.
- **LCD 성능 극대화 및 안정화**:
    - **더블 버퍼링(Double Buffering) 및 DMA 적용**: 320라인 싱글 버퍼를 160라인 x 2 분할 버퍼로 재구성하고 SPI DMA를 활성화하여, 렌더링과 전송을 병렬로 처리함에 따라 화면 훑음(Scanline) 현상을 제거하고 시각적 부드러움 확보.
    - **클럭 최적화**: CPU 클럭을 240MHz(S3 최대)로, SPI 클럭을 80MHz(하드웨어 한계치)로 상향하여 전체 화면 업데이트에서도 안정적인 30 FPS를 유지하도록 최적화.
    - **프레임 레이트 정확도 개선**: 단순 전송 횟수(Partial flush)가 아닌 LVGL 내부의 평균 논리 프레임(`lv_refr_get_fps_avg`) 측정 방식으로 전환하여 실제 체감 성능을 정확히 표기하도록 수정.
    - **설정 병목 해결**: `lv_conf.h`의 주사율 제한(50ms) 설정을 30 FPS 타겟(33ms)에 맞게 정렬하여 소프트웨어적 제약 해소.
- **문서화 최종화**: `docs/lcd.md`와 `docs/todo.md`에 최종 하드웨어 설정값, 성능 지표, 그리고 더블 버퍼링 구현 세부 사항을 최신화하여 운영 가이드 완성.

## 2026-03-24
- **LCD DMA 및 더블 버퍼링 원복**:
    - Non-blocking 전송(DMA) 시 화면 업데이트가 상시적으로 발생하지 않는 문제를 해결하기 위해 이전의 안정적인 싱글 버퍼(20라인) 및 블로킹 전송 방식으로 원복.
    - `eth2ap.ino`: 160라인 x 2 더블 버퍼를 20라인 싱글 버퍼로 변경하여 동기식 SPI 전송 안정성 확보.
    - `eth2ap.ino`: `pushImageDMA` 및 `dmaWait` 코드로 인한 비동기 충돌 가능성을 제거하고 `pushColors` 기반의 블로킹 전송으로 복구.
    - `eth2ap.ino`: `tft.initDMA()` 호출을 제거하여 하드웨어 리소스 최적화.
    - **LCD 성능 극대화 (18 FPS -> 53 FPS / 300% 향상)**:
    - SPI 클럭 80MHz 상향 및 비동기 DMA(`tft.endWrite()` 제거) 적용으로 병렬 처리 극대화.
    - ESP32-S3의 32KB L1 캐시에 맞춘 **64라인 더블 버퍼링**으로 CPU 연산 효율 100% 달성.
    - `lv_conf.h` 가드 추가 및 `LV_DISP_DEF_REFR_PERIOD=16` 설정으로 소프트웨어 주사율 제한 해제.
    - 40MHz vs 80MHz 비교 벤치마크 수행으로 성능 병목 구간(SPI 대역폭) 확인.
    - 최종 성능: **53 FPS (CPU 82%)**의 안정적인 고주사율 환경 구축 완료.

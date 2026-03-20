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
    - 모든 문서 업데이트 시 **한글** 사용 원칙을 모든 스킬(`/commit`, `/load`)에 반영.
    - `progress.md` 기존 내용을 한글로 전체 번역 완료.
- **LCD SPI 통신 및 진단 로직 개선**:
    - `detectLCD()` 함수에 하드웨어 리셋(`LCD_RST_PIN`), DC 핀 제어, MISO 내부 풀업 설정을 추가하여 레지스터(0x04) 읽기 신뢰성 향상.
    - 디버깅을 위해 SPI 프로빙 클럭 속도를 1MHz로 하향 조정.
    - `utilities.h`에 `ENABLE_LCD` 매크로 정의 및 관련 기능 활성화.
- **하드웨어 디버깅 계획 수립**:
    - `todo.md`에 오실로스코프를 이용한 SPI 신호(SCLK, MOSI, MISO, RST, CS) 정밀 분석 계획 반영.

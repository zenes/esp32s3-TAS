# tas - 이더넷-WiFi AP 브리지

## 개요

이 예제는 T-ETH-Lite-ESP32S3 보드를 사용하여 이더넷과 WiFi AP(액세스 포인트) 간의 브리지를 구현합니다. 이더넷 포트에 연결된 네트워크를 WiFi를 통해 다른 장치들과 공유할 수 있습니다.

## 주요 기능

- **부팅 시 WiFi AP 자동 시작**: 이더넷 연결 여부와 상관없이 WiFi AP가 항상 켜져 있어 접속이 가능합니다
- **NAPT 기반 패킷 포워딩**: 이더넷이 연결되면 인터넷 공유 기능이 자동으로 활성화됩니다
- **유연한 연결 관리**: 이더넷 연결이 끊어져도 WiFi AP는 유지되므로 클라이언트 연결이 끊기지 않습니다
- **상태 모니터링**: 30초마다 연결 상태를 시리얼 모니터에 출력합니다

## WiFi AP 설정

- **SSID**: `tas`
- **비밀번호**: `12345678`
- **채널**: 1
- **최대 연결 수**: 4개 장치

## 하드웨어 요구사항

- LilyGO T-ETH-Lite-ESP32S3 보드
- 이더넷 케이블
- DHCP 서버가 있는 네트워크 (라우터 등)

## 소프트웨어 요구사항

### Arduino Core 버전별 기능

| Arduino ESP32 Core | 빌드 | NAPT 지원 | 브리지 기능 |
|-------------------|------|----------|-----------|
| 2.x               | ✅   | ❌       | 제한적     |
| 3.0.0 이상        | ✅   | ✅       | 완전      |

> **중요**: 완전한 브리지 기능을 사용하려면 Arduino ESP32 Core 3.0.0 이상이 필요합니다.

## 사용 방법

### 1. 업로드

PlatformIO를 사용하는 경우:
```bash
# platformio.ini에서 src_dir = examples/tas 설정
pio run -e T-ETH-Lite-ESP32S3 -t upload
```

Arduino IDE를 사용하는 경우:
1. `examples/tas/tas.ino` 파일 열기
2. 보드: ESP32-S3 Dev Module 선택
3. 업로드

### 2. 연결

1. T-ETH-Lite-ESP32S3 보드에 전원 연결
2. 이더넷 케이블을 보드와 라우터에 연결
3. 시리얼 모니터(115200 baud)에서 연결 상태 확인

### 3. WiFi 연결

1. 스마트폰이나 노트북에서 WiFi 설정 열기
2. SSID `tas` 찾기
3. 비밀번호 `12345678` 입력하여 연결
4. 인터넷 접속 테스트

## 시리얼 모니터 출력 예시

### 시작 시
```
===== tas Bridge Example =====
Ethernet to WiFi AP Bridge using NAPT
=====================================

Initializing Ethernet...
Ethernet initialized successfully
Waiting for Ethernet connection...
```

### 이더넷 연결 시
```
ETH Started
ETH Connected
ETH MAC: AA:BB:CC:DD:EE:FF, IPv4: 192.168.1.100, FULL_DUPLEX, 100Mbps
Starting WiFi AP...
WiFi AP Started
AP SSID: tas
AP IP address: 192.168.4.1
NAPT enabled - Bridge is active
```

### WiFi 클라이언트 연결 시
```
WiFi AP: Station connected
```

### 주기적 상태 출력 (30초마다)
```
----- Status -----
Ethernet: Connected
  IP: 192.168.1.100
WiFi AP: Running
  Connected Stations: 2
------------------
```

## 문제 해결

### 이더넷이 연결되지 않는 경우
- 이더넷 케이블이 제대로 연결되어 있는지 확인
- 네트워크에 DHCP 서버가 있는지 확인
- 시리얼 모니터에서 에러 메시지 확인

### WiFi AP가 보이지 않는 경우
- 부팅 직후 "Starting WiFi AP..." 및 "WiFi AP Started!" 메시지가 시리얼에 뜨는지 확인
- 이더넷 연결과 상관없이 AP는 항상 보여야 합니다

### 인터넷이 안 되는 경우
- Arduino ESP32 Core 버전 확인 (3.0.0 이상 필요)
- 시리얼 모니터에서 "NAPT enabled" 메시지 확인
- Core 2.x 사용 시 경고 메시지가 표시됩니다

## 기술적 세부사항

### 원본 ESP-IDF 예제와의 차이점

| 항목 | ESP-IDF 원본 | Arduino 버전 |
|-----|-------------|-------------|
| 동작 계층 | Layer 2 (MAC) | Layer 3 (IP) |
| 패킷 포워딩 | 직접 전달 | NAPT |
| TCP/IP 스택 | 비활성화 | 활성화 |
| 구현 복잡도 | 높음 | 낮음 |

Arduino 버전은 NAPT를 사용하여 더 간단하고 유지보수가 쉬운 구현을 제공합니다.

## 라이선스

MIT License

## 참고

- 원본 ESP-IDF 예제: [esp-idf/examples/ethernet/tas](https://github.com/espressif/esp-idf)
- LilyGO 제품 페이지: [T-ETH-Lite](https://www.lilygo.cc/products/t-eth-lite)

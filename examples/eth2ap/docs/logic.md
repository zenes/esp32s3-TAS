# eth2ap 프로그램 로직 설명

## 전체 구조

```
┌─────────────────┐
│ Device A        │ (IP: 192.168.0.100)
│ (RTPS Server)   │ ◄─── Ethernet Cable
└────────┬────────┘
         │
         │ Direct Ethernet Connection
         │
┌────────▼────────┐
│   ESP32-S3      │ (Ethernet IP: 192.168.0.50)
│                 │
│  ┌───────────┐  │
│  │   NAPT    │  │ ◄─── Packet Translation
│  │  (Layer 3)│  │
│  └───────────┘  │
│                 │
└────────┬────────┘
         │
         │ WiFi (SSID: eth2ap)
         │
┌────────▼────────┐
│   Device B      │
│ (RTPS Client)   │
└─────────────────┘
```

## 주요 컴포넌트

### 1. 초기화 단계 (setup 함수)

```cpp
void setup() {
    // 1. WiFi AP 즉시 시작 (항상 표시)
    WiFi.softAP(AP_SSID, AP_PASSWORD, ...);
    
    // 2. 이벤트 핸들러 등록
    WiFi.onEvent(NetworkEvent);
    
    // 3. 이더넷 초기화
    ETH.begin(...);
}
```

**동작 순서:**
1. 시리얼 초기화
2. **WiFi AP를 먼저 시작**하여 장치들이 바로 접속할 수 있게 함
3. WiFi/Ethernet 이벤트 핸들러 등록
4. W5500 이더넷 초기화 및 연결 대기

### 2. 이벤트 처리 (NetworkEvent 함수)

#### 2.1 이더넷 이벤트

```cpp
case ARDUINO_EVENT_ETH_GOT_IP:
    // 이더넷 연결 및 IP 획득 시
    // NAPT 활성화 (인터넷 공유 시작)
    WiFi.AP.enableNAPT(true);
```

**변경점**: 이전에는 여기서 AP를 시작했으나, 이제는 NAPT(인터넷 공유 기능)만 활성화합니다.

```cpp
case ARDUINO_EVENT_ETH_DISCONNECTED:
    // 이더넷 연결 끊김
    // NAPT 비활성화 (AP는 유지됨)
    WiFi.AP.enableNAPT(false);
```

#### 2.2 WiFi AP 이벤트

```cpp
case ARDUINO_EVENT_WIFI_AP_START:
    // WiFi AP 시작 완료
    // AP 정보 출력 (SSID, IP)
```

```cpp
case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
    // 클라이언트 장치 연결
    Serial.println("WiFi AP: Station connected");
```

```cpp
case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
    // 클라이언트 장치 연결 해제
```

### 3. 메인 루프 (loop 함수)

```cpp
void loop() {
    // 30초마다 상태 출력
    if (currentMillis - lastStatusPrint >= 30000) {
        // 이더넷 연결 상태
        // WiFi AP 상태
        // 연결된 클라이언트 수
    }
    delay(100);
}
```

**역할:**
- 주기적으로 시스템 상태 모니터링
- 사용자에게 현재 상태 정보 제공

## 패킷 흐름

### 인터넷 → WiFi 클라이언트

```
[인터넷] 
   ↓
[라우터] (192.168.1.1)
   ↓
[이더넷] → [ESP32-S3] → [NAPT 변환] → [WiFi AP] → [클라이언트]
           (192.168.1.100)              (192.168.4.1)   (192.168.4.2)
```

**NAPT 동작:**
1. 클라이언트(192.168.4.2)가 인터넷 요청
2. ESP32-S3가 소스 IP를 192.168.1.100으로 변환
3. 라우터로 패킷 전송
4. 응답 패킷 수신 시 다시 192.168.4.2로 변환하여 전달

### WiFi 클라이언트 → 인터넷

```
[클라이언트] (192.168.4.2)
   ↓
[WiFi AP] (192.168.4.1)
   ↓
[NAPT 변환] (192.168.4.2 → 192.168.1.100)
   ↓
[이더넷] (192.168.1.100)
   ↓
[라우터] (192.168.1.1)
   ↓
[인터넷]
```

## 상태 관리

### 전역 변수

```cpp
static bool eth_connected = false;     // 이더넷 연결 상태
static bool ap_started = false;        // WiFi AP 시작 상태
```

### 상태 전이도

```
[초기 상태]
    ↓
[WiFi AP 시작] (항상 켜짐)
    ↓
[이더넷 초기화]
    ↓
[이더넷 연결 대기] ←──────┐
    ↓                      │
[이더넷 연결됨]             │
    ↓                      │
[IP 주소 획득]             │
    ↓                      │
[NAPT 활성화] (인터넷 공유) │
    ↓                      │
[브리지 동작 중] ──────────┤
    ↓                      │
[이더넷 연결 끊김] ─────────┘
    ↓
[NAPT 비활성화] (AP는 계속 유지)
```

## 버전별 차이점

### Arduino Core 2.x

```cpp
#if ESP_ARDUINO_VERSION < ESP_ARDUINO_VERSION_VAL(3,0,0)
    // ETHClass2 사용 (lib 폴더의 커스텀 라이브러리)
    #include <ETHClass2.h>
    #define ETH ETH2
    
    // NAPT 미지원 - 경고 메시지만 출력
    Serial.println("WARNING: NAPT is not supported");
#endif
```

### Arduino Core 3.x

```cpp
#else
    // 표준 ETH 라이브러리 사용
    #include <ETH.h>
    
    // NAPT 지원
    WiFi.AP.enableNAPT(true);
#endif
```

## 핀 설정 (T-ETH-Lite-ESP32S3)

### W5500 SPI 연결

```
ESP32-S3          W5500
─────────────────────────
GPIO 10  (SCLK) → SCLK
GPIO 11  (MISO) ← MISO
GPIO 12  (MOSI) → MOSI
GPIO 9   (CS)   → CS
GPIO 13  (INT)  ← INT
GPIO 14  (RST)  → RST
```

### SPI 버스

- **SPI3_HOST** 사용 (ESP32-S3의 세 번째 SPI 버스)
- **PHY 주소**: 1 (W5500 칩 주소)

## 성능 특성

### 메모리 사용량

- **RAM**: 43,400 bytes (13.2%)
- **Flash**: 723,801 bytes (23.0%)

### 네트워크 성능

- **이더넷 속도**: 최대 100Mbps (Full Duplex)
- **WiFi 속도**: 802.11 b/g/n (2.4GHz)
- **최대 동시 연결**: 4개 장치

## 에러 처리

### 이더넷 초기화 실패

```cpp
if (!ETH.begin(...)) {
    Serial.println("ETH start Failed!");
    return;  // setup 함수 종료
}
```

### NAPT 활성화 실패

```cpp
if (WiFi.AP.enableNAPT(true)) {
    Serial.println("NAPT enabled - Bridge is active");
} else {
    Serial.println("NAPT enable failed!");
}
```

## 디버깅 팁

1. **시리얼 모니터 확인**: 모든 주요 이벤트가 로그로 출력됩니다
2. **30초 상태 출력**: 주기적으로 연결 상태 확인 가능
3. **이더넷 LED**: 보드의 LED로 물리적 연결 확인
4. **WiFi 스캔**: 스마트폰에서 AP가 보이는지 확인

## 확장 가능성

### 설정 변경

```cpp
// WiFi AP 설정 커스터마이징
#define AP_SSID "내_네트워크"
#define AP_PASSWORD "내_비밀번호"
#define AP_CHANNEL 6
#define AP_MAX_CONN 8
```

### 추가 기능 아이디어

- 웹 서버를 통한 설정 UI
- 트래픽 모니터링 및 통계
- 방화벽 기능
- QoS (Quality of Service)
- VPN 지원

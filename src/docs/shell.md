# Serial Debug Shell 명령어 가이드

본 문서는 `eth2ap` 프로젝트의 시리얼 터미널 인터페이스에서 사용할 수 있는 모든 명령어의 상세 설명, 실제 입력 예시, 예상 출력 결과 및 각 지표의 의미를 안내합니다.

---

## 시스템 기본 명령어

### 1. `help`
사용 가능한 모든 명령어 목록과 간략한 설명을 출력합니다.
- **입력 예시:** `help`
- **예상 결과:**
  ```text
  Available Commands:
    help            - Show this help
    status          - Show current system status
    ... (중략) ...
    udp_iperf <start|stop>- Start/stop UDP Speed Test (Port 5002)
  ```
- **의미:** 현재 펌웨어 버전에서 지원하는 모든 명령어 셋을 한눈에 확인할 수 있습니다.

### 2. `status`
시스템의 실시간 상태 정보를 요약하여 보여줍니다.
- **입력 예시:** `status`
- **예상 결과:**
  ```text
  --- System Status ---
  Ethernet: Connected (IP: 192.168.0.50)
  WiFi AP : Running (SSID: eth2ap)
  ...
  Free RAM: 180 KB (Internal)
  PSRAM   : 4096 / 8192 KB (Free/Total)
  -----------------------------
  ```
- **결과값 의미:**
  - **Uptime:** 장치가 부팅된 후 흐른 시간(초)입니다.
  - **CPU Temp:** 현재 ESP32 칩의 온도로, 과부하 여부를 판단하는 지표입니다.
  - **Free RAM/PSRAM:** 남은 메모리로, 시스템 안정성을 체크할 때 중요합니다.

### 3. `restart`
시스템을 즉시 재부팅합니다.
- **입력 예시:** `restart`
- **예상 결과:**
  ```text
  Restarting system...
  ```
- **의미:** 설정 변경 사항을 안전하게 적용하거나 시스템을 초기화하기 위해 장치를 다시 시작합니다.

### 4. `loglevel`
실시간 로그 출력 수준을 조정합니다.
- **입력 예시:** `loglevel 4`
- **예상 결과:**
  ```text
  Log level changed to 4
  ```
- **의미:** 터미널에 출력되는 로그의 상세도를 조절합니다. (0:NONE ~ 4:DEBUG)

### 5. `dmesg`
메모리에 기록된 최근 시스템 로그를 출력합니다.
- **입력 예시:** `dmesg 10`
- **예상 결과:**
  ```text
  --- System Logs (dmesg) ---
  [   120.450] [INFO ] ETH Connected
  ... (최근 10줄 출력) ...
  ```
- **의미:** 이미 지나간 중요한 시스템 이벤트나 오류 로그를 다시 확인할 때 사용합니다.

---

## 구성 및 설정 명령어

### 6. `set_ssid`
WiFi AP의 이름(SSID)을 설정하고 저장합니다.
- **입력 예시:** `set_ssid MyBridge-01`
- **예상 결과:**
  ```text
  Changing SSID to 'MyBridge-01'. Restarting AP...
  ```
- **의미:** AP의 이름을 영구적으로 변경합니다. (NVS 저장)

### 7. `set_pw`
WiFi AP의 접속 비밀번호를 설정하고 저장합니다.
- **입력 예시:** `set_pw 87654321`
- **예상 결과:**
  ```text
  Changing Password to '87654321'. Restarting AP...
  ```
- **의미:** AP의 보안을 위해 비밀번호를 변경합니다. (최소 8자 필요)

---

## 패킷 분석 및 진단

### 8. `ifconfig` (또는 `ip`)
네트워크 인터페이스 상세 구성을 확인합니다.
- **입력 예시:** `ifconfig`
- **예상 결과:**
  ```text
  [ETH] Ethernet Interface:
    IPv4  : 192.168.0.50
    GW    : 192.168.0.1
    ...
  ```
- **결과값 의미:**
  - **GW (Gateway):** 외부 네트워크로 나가는 통로입니다.
  - **MAC:** 장치의 고유 물리 주소입니다.

### 9. `stats`
인터페이스별 데이터 전송 누적 통계입니다.
- **입력 예시:** `stats`
- **예상 결과:**
  ```text
  [Ethernet]
    RX: 1.25 MB (12500 pkts)
    TX: 450 KB (5000 pkts)
  ```
- **결과값 의미:** 장치가 부팅된 후 얼마나 많은 데이터와 패킷이 처리되었는지 보여줍니다.

### 10. `monitor`
주기적 상태 감시 모드를 켜거나 끕니다.
- **입력 예시:** `monitor on`
- **예상 결과:**
  ```text
  Periodic monitoring: ON
  ```
- **의미:** 시리얼 창에 주기적으로 시스템 상태를 리포트하여 실시간 모니터링을 돕습니다.

### 11. `traffic`
실시간 트래픽 로그를 활성화하거나 기기 목록을 봅니다.
- **입력 예시:** `traffic on`
- **예상 결과:**
  ```text
  Real-time traffic logging: ON
  ```
- **의미:** NAT를 통해 전달되는 패킷의 흐름을 실시간 로그로 확인할 수 있습니다.

---

## 네트워크 유틸리티

### 12. `ping`
네트워크 연결성을 확인합니다.
- **입력 예시:** `ping 8.8.8.8`
- **예상 결과:**
  ```text
  Reply from 8.8.8.8: time=32.50ms
  ```
- **결과값 의미:** **time(Latency)** 값이 낮고 일정할수록 네트워크 연결이 양호한 것입니다.

### 13. `arp`
현재 연결된 스테이션 기기 목록을 확인합니다.
- **입력 예시:** `arp`
- **예상 결과:**
  ```text
  --- AP ARP / Station List ---
    [1] MAC: 12:34:56:AB:CD:EF, RSSI: -45
  ```
- **결과값 의미:** **RSSI**가 `-30`에 가까울수록 신호가 강하고, `-80` 이하면 불안정합니다.

### 14. `dhcp`
할당된 IP 주소 정보를 확인합니다.
- **입력 예시:** `dhcp`
- **예상 결과:**
  ```text
  --- AP DHCP Leases ---
    [1] MAC: 12:34:56:AB:CD:EF  ->  IP: 192.168.4.2
  ```
- **의미:** 접속된 클라이언트에게 어떤 IP가 할당되었는지 정확히 파악할 수 있습니다.

---

## 성능 테스트 (Speed Test)

### 15. `iperf`
TCP iPerf 서버를 제어합니다.
- **입력 예시:** `iperf start`
- **예상 결과:**
  ```text
  iPerf TCP server started (Port 5001).
  ```
- **의미:** 네트워크 대역폭을 정밀하게 측정하기 위해 ESP32를 TCP 서버로 동작시킵니다.

### 16. `udp_iperf`
UDP 전용 속도 측정 서버를 제어합니다.
- **입력 예시:** `udp_iperf start`
- **예상 결과:**
  ```text
  UDP Test server started on Port 5002.
  ```
- **의미:** 가벼운 UDP 패킷 수신을 통해 시스템의 최대 패킷 처리 속도를 측정합니다.

---

## 편리 기능 (고급 사용자)
- **탭 자동 완성 (Tab):** 명령어 자동 완성
- **히스토리 (↑ / ↓):** 이전 명령어 불러오기
- **중단 (Ctrl+C):** 진행 중인 명령어(ping 등) 강제 종료

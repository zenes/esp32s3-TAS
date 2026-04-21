#ifndef CPU_STATS_H
#define CPU_STATS_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_freertos_hooks.h"
#include "esp_timer.h"

// ============================================================
// Idle hook counters (IRAM으로 인터럽트 레이턴시 최소화)
// ============================================================
static volatile uint32_t idle_ticks[2] = {0, 0};

static bool IRAM_ATTR _idle_hook_0() { idle_ticks[0]++; return true; }
static bool IRAM_ATTR _idle_hook_1() { idle_ticks[1]++; return true; }

/**
 * @brief CPUStats — Idle Hook + 10초 Warm-up 방식 per-core CPU 부하 측정
 *
 * 동작 원리:
 *   1. begin() → Idle Hook 등록, Warm-up 타이머 시작
 *   2. 첫 10초 동안 update()가 호출될 때마다 idle tps 최댓값을 추적 (baseline 자동 탐색)
 *   3. 10초 후 baseline을 고정(lock) → 이후부터 실제 부하율 보고
 *
 * 이 접근의 이점:
 *   - 캘리브레이션 타이밍에 민감하지 않음 (시스템이 스스로 최대 idle 상태를 탐색)
 *   - WiFi 초기화, NVS, LCD 초기화 등 부팅 burst가 모두 warm-up 안에 수렴됨
 *   - 특정 시점에 baseline을 잡는 방식의 왜곡 제거
 *
 * 한계 (투명하게 공개):
 *   - WiFi ISR 실행 시간은 측정 불가 (Idle Hook 방식의 근본 한계)
 *   - Core 0 WiFi 트래픽이 없으면 0% 로 표시되는 것이 CORRECT한 동작
 *   - Arduino ESP32 2.0.14 SDK에서 uxTaskGetSystemState 미지원으로 인한 차선책
 */
class CPUStats {
private:
    uint32_t  _last_ticks[2]    = {0, 0};
    int64_t   _last_update_us   = 0;
    float     _baseline_tps[2]  = {0.0f, 0.0f}; // warm-up 동안 누적된 최대 idle tps
    float     _last_load[2]     = {0.0f, 0.0f};
    uint32_t  _warmup_start_ms  = 0;
    bool      _hooks_registered = false;
    bool      _baseline_locked  = false;

    // warm-up 시간: 이 기간 동안 max idle tps를 추적하여 baseline 확정
    static const uint32_t WARMUP_MS = 10000;
    // 루프 최소 간격 (너무 자주 호출 방지)
    static const int64_t  MIN_INTERVAL_US = 900000; // 900ms

    void registerHooks() {
        if (!_hooks_registered) {
            esp_register_freertos_idle_hook_for_cpu(_idle_hook_0, 0);
            esp_register_freertos_idle_hook_for_cpu(_idle_hook_1, 1);
            _hooks_registered = true;
        }
    }

public:
    CPUStats() {}

    /**
     * @brief Idle Hook 등록 및 warm-up 타이머 시작. setup() 초입에서 호출.
     * WiFi / Ethernet 시작 전후 어디에 위치해도 동작함 (10초 warm-up이 흡수).
     */
    void begin() {
        registerHooks();
        _last_ticks[0]   = idle_ticks[0];
        _last_ticks[1]   = idle_ticks[1];
        _last_update_us  = esp_timer_get_time();
        _warmup_start_ms = millis();
        _baseline_locked = false;
        Serial.println("[CPU] Idle Hook 등록. 10초 warm-up 시작...");
    }

    /**
     * @brief 매 1초마다 loop()에서 호출. 부하율 갱신.
     */
    void update() {
        if (!_hooks_registered) return;

        int64_t now_us = esp_timer_get_time();
        int64_t elapsed_us = now_us - _last_update_us;
        if (elapsed_us < MIN_INTERVAL_US) return;

        // 현재 idle ticks 캡처 (atomic-safe: 32-bit read on Xtensa는 atomic)
        uint32_t cur[2] = { idle_ticks[0], idle_ticks[1] };
        float tps[2];
        for (int i = 0; i < 2; i++) {
            uint32_t delta = cur[i] - _last_ticks[i];
            tps[i] = (float)delta * 1000000.0f / (float)elapsed_us;
        }

        if (!_baseline_locked) {
            // ── Warm-up 구간: 최댓값 추적 ──────────────────────
            for (int i = 0; i < 2; i++) {
                if (tps[i] > _baseline_tps[i]) {
                    _baseline_tps[i] = tps[i];
                }
            }

            if (millis() - _warmup_start_ms >= WARMUP_MS) {
                _baseline_locked = true;
                Serial.printf("[CPU] Baseline 확정: Core0=%.0f tps, Core1=%.0f tps\n",
                               _baseline_tps[0], _baseline_tps[1]);
            }
            // warm-up 중에는 부하율 미보고 (0.0 유지)
        } else {
            // ── 정상 측정 구간 ───────────────────────────────
            for (int i = 0; i < 2; i++) {
                if (_baseline_tps[i] <= 0.0f) continue;
                float idle_pct = tps[i] * 100.0f / _baseline_tps[i];
                _last_load[i] = 100.0f - idle_pct;
                if (_last_load[i] < 0.0f)   _last_load[i] = 0.0f;
                if (_last_load[i] > 100.0f) _last_load[i] = 100.0f;
            }
        }

        _last_ticks[0]  = cur[0];
        _last_ticks[1]  = cur[1];
        _last_update_us = now_us;
    }

    /**
     * @brief Core별 부하율(%) 반환. warm-up 중에는 0.0f.
     */
    float getLoad(int core) {
        if (core < 0 || core > 1) return 0.0f;
        return _last_load[core];
    }

    /**
     * @brief warm-up 완료 여부.
     */
    bool isReady() const { return _baseline_locked; }

    /**
     * @brief 시리얼 시스템 모니터 출력 (top 명령어용).
     */
    void showTop() {
        update();

        Serial.print("\033[2J\033[H"); // 화면 클리어 + 커서 홈 (VT100/PlatformIO 모니터용)
        Serial.println("=======================================================");
        Serial.println(" ESP32-S3 SYSTEM MONITOR");
        if (!_baseline_locked) {
            uint32_t remaining = WARMUP_MS - (millis() - _warmup_start_ms);
            Serial.print(" [Warm-up: ");
            Serial.print(remaining / 1000 + 1);
            Serial.println("s remaining]");
        } else {
            Serial.print(" [Baseline C0=");
            Serial.print((int)_baseline_tps[0]);
            Serial.print(" / C1=");
            Serial.print((int)_baseline_tps[1]);
            Serial.println(" tps]");
        }
        Serial.println("-------------------------------------------------------");
        if (_baseline_locked) {
            Serial.print(" Core 0 Load  : ");
            Serial.print(_last_load[0], 1);
            Serial.println(" %  (sys/net)");
            Serial.print(" Core 1 Load  : ");
            Serial.print(_last_load[1], 1);
            Serial.println(" %  (app/ui)");
        } else {
            Serial.println(" Core 0 Load  : -- (warming up)");
            Serial.println(" Core 1 Load  : -- (warming up)");
        }
        Serial.println("-------------------------------------------------------");
        Serial.print(" Uptime       : ");
        Serial.print(millis() / 1000);
        Serial.println(" s");
        Serial.print(" Task Count   : ");
        Serial.println((uint32_t)uxTaskGetNumberOfTasks());
        Serial.print(" Free Heap    : ");
        Serial.print(ESP.getFreeHeap() / 1024);
        Serial.println(" KB");
        if (psramFound()) {
            Serial.print(" PSRAM Free   : ");
            Serial.print(ESP.getFreePsram() / 1024);
            Serial.print(" / ");
            Serial.print(ESP.getPsramSize() / 1024);
            Serial.println(" KB");
        }
        Serial.print(" CPU Temp     : ");
        Serial.print(temperatureRead(), 1);
        Serial.println(" C");
        Serial.println("=======================================================");
        Serial.print("> ");
    }

    void clear() {}
};

#endif

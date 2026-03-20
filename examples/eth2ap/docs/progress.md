# Project Progress

## 2026-03-20
- **Ethernet Isolation for LCD Debugging**:
    - Added `ENABLE_ETHERNET` toggle in `utilities.h` (disabled by default to prevent conflicts).
    - Wrapped Ethernet initialization, event handlers, and loop logic in `eth2ap.ino` with `#ifdef ENABLE_ETHERNET`.
    - Wrapped Ethernet-specific shell commands (`stats`, `iperf`, `dhcp`, etc.) in `eth2ap.ino`.
- **Build & Compatibility Fixes**:
    - Corrected `platformio.ini` source filter to include all source files (`+<*>`).
    - Restored missing libraries (`ETHClass2`, `ESP32Ping`, `lvgl`) by removing them from `lib_ignore`.
    - Fixed Arduino Core 2.0.x compatibility issues in `eth2ap.ino` (specifically `WiFi.AP.netif()`).
    - Switched to `lv_font_montserrat_14` for LCD title to resolve font linking errors.
- **Documentation & Workflow**:
    - Created `docs/spi.md` with detailed SPI channel analysis and configuration.
    - Updated `docs/todo.md` to track LCD implementation progress.
    - Registered and executed the `/commit` workflow for automated documentation and release.

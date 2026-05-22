# ESP32 Firmware

ESP32-S3 firmware for the AI Companion Robot. Handles WiFi, audio I/O, servo, LCD, and cloud AI communication.

> Current dev board: ESP32-C6. Target: ESP32-S3-WROOM-1-N16R8.

---

## Structure

```
esp32/
├── components/         # Reusable modules (one folder per component)
│   ├── bsp/            # Board Support Package — all GPIO pin definitions
│   ├── led_driver/     # WS2812 RGB LED
│   ├── wifi_manager/   # WiFi station connection
│   ├── http_server/    # REST debug endpoints
│   ├── i2s_driver/     # Audio I/O (INMP441 mic + MAX98357A amp)     [todo]
│   ├── cloud_api/      # Claude API + STT + TTS                       [todo]
│   ├── audio_pipeline/ # Record → STT → Claude → TTS → Playback      [todo]
│   ├── servo_driver/   # MG90S servo control                          [todo]
│   ├── lcd_driver/     # 1.9" 170x320 display                         [todo]
│   ├── emotion_engine/ # Emotion state machine                        [todo]
│   └── uart_bridge/    # K230 ↔ ESP32 JSON protocol                   [todo]
├── examples/           # Reference code (do not modify)
│   ├── blinky/         # LED blink demo
│   └── wifi_demo/      # WiFi + HTTP server + LED
├── main/               # app_main — orchestration only, no logic
├── tests/
│   ├── unit/           # Host-based tests, no hardware needed
│   └── integration/    # On-target tests, runs on ESP32
├── CMakeLists.txt
└── sdkconfig.defaults  # Config placeholders (safe to commit)
```

---

## Quick Start

```bash
# 1. Set WiFi credentials
idf.py menuconfig
# Navigate to: AI Companion Configuration → set SSID and Password

# 2. Build and flash
idf.py build
idf.py flash monitor
```

---

## Component Rules

- Each component exposes a public API via `<name>.h` only
- All GPIO pin numbers defined in `bsp/bsp_board.h` — never hardcoded elsewhere
- Components do not depend on each other directly — use FreeRTOS queues for cross-component data
- `main.c` only calls `_init()` functions — zero business logic

## Test Rules

- Write the test before writing the implementation (TDD)
- `tests/unit/` — pure C logic, compiles on host PC, no ESP-IDF hardware calls
- `tests/integration/` — runs on ESP32, tests real hardware behavior
- Test file naming: `test_<component_name>.c`

---

## WiFi Credentials

Credentials are set via `idf.py menuconfig` and stored in `sdkconfig` (gitignored).  
To share defaults with teammates: copy `sdkconfig.defaults` → `sdkconfig.defaults.local` and fill in real values. Never commit `sdkconfig.defaults.local`.

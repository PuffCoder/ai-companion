# CLAUDE.md — AI Companion Robot

## 🔒 SECRETS RULE (strict)
- NEVER read files containing credentials: `config.h`, `config.py`, `sdkconfig.defaults.local`, any file with WiFi SSID/password or API keys.
- NEVER print, echo, or log WiFi credentials or API keys.
- The USER fills in all credentials themselves. Claude only touches the surrounding code (buffer sizes, header construction, etc.), never the secret values.

## Project Overview
ESP32-C6 (prototype) → ESP32-S3 + K230-CanMV (production) AI companion robot.
Language: C with ESP-IDF v5.x + FreeRTOS.
Development style: TDD with Unity test framework, modular component architecture.

## Developer Background
- Experienced: C, FreeRTOS, I2C, SPI, UART
- Learning: DMA, I2S audio, HTTP client on embedded, K230/MicroPython
- Goal: portfolio project for internship applications

## Project Structure
```
ai-companion/
├── esp32/                  # ESP32-S3 firmware — MAIN DEVELOPMENT
│   ├── components/         # Modular components (one folder per module)
│   ├── main/               # app_main only — orchestration, no logic
│   ├── tests/
│   │   ├── unit/           # Host-based tests (no hardware needed)
│   │   └── integration/    # On-target tests
│   ├── CMakeLists.txt
│   └── sdkconfig.defaults  # Safe placeholder — real creds in sdkconfig.defaults.local (gitignored)
├── esp32/examples/         # Reference examples (do not modify)
│   ├── blinky/
│   └── wifi_demo/
├── k230/                   # K230-CanMV MicroPython code
│   ├── vision/
│   └── wake_word/
├── shared/protocol/        # UART JSON protocol (used by both boards)
└── docs/
```

## Build Commands
```bash
cd esp32
idf.py build
idf.py flash monitor
idf.py menuconfig                     # configure WiFi SSID/password
idf.py -T tests/unit build            # host unit tests
idf.py -T tests/integration flash     # on-target tests
```

## WiFi Credentials
- Set via `idf.py menuconfig` → AI Companion Configuration
- Or: copy `sdkconfig.defaults` → `sdkconfig.defaults.local` and fill in real values
- `sdkconfig.defaults.local` is gitignored — never commit real credentials

## Architecture Rules
- Components live in `esp32/components/<name>/` — each has `<name>.c`, `<name>.h`, `CMakeLists.txt`
- Layering: app → services → drivers → BSP. Never reverse or skip layers.
- BSP (`bsp_board.h`) owns ALL GPIO pin definitions — never hardcode pins in components
- Components communicate via FreeRTOS queues (async) or direct calls (sync, same layer only)
- `main.c` only calls init functions — zero business logic

## TDD Rules
- Write test file before implementation
- Unit tests: `tests/unit/test_<component>.c` — must compile and run on host (no ESP-IDF hardware deps)
- Integration tests: `tests/integration/test_<component>.c` — runs on target board
- Test naming: `test_<what_it_does>(void)`

## Code Style
- Comments in English
- Log tag per file: `static const char *TAG = "<component_name>";`
- Use `ESP_ERROR_CHECK()` for `esp_err_t` returns
- Use `ESP_LOGI/LOGE/LOGW` — never `printf` inside components

## Key Components Status
| Component     | Status      | Notes |
|---------------|-------------|-------|
| led_driver    | Done        | WS2812 via RMT |
| wifi_manager  | Done        | Kconfig credentials (fixed) |
| http_server   | Done        | /led/on /led/off |
| bsp           | Stub        | Needs GPIO defs for S3 target |
| i2s_driver    | Not started | INMP441 mic + MAX98357A amp |
| cloud_api     | Not started | Claude API + STT + TTS |
| audio_pipeline| Not started | Orchestrates i2s + cloud_api |
| lcd_driver    | Not started | 170x320 ISP |
| servo_driver  | Not started | MG90S |
| emotion_engine| Not started | State machine → LED/servo/LCD |
| uart_bridge   | Done        | UART1 GPIO17/18 ↔ K230 UART1 GPIO3/4, NDJSON; bidirectional HW verified |

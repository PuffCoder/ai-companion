# AI Companion Robot (小黄人陪伴机器人)

An AI-powered companion robot built on ESP32-C6 (prototype) → ESP32-S3 + K230-CanMV (production), featuring voice interaction, emotion expression, memory, and Claude AI integration.

Built as a learning project and portfolio piece, developed using TDD (Test-Driven Development) principles with ESP-IDF and Unity test framework.

---

## Hardware

### Prototype (Current)
| Component | Part |
|-----------|------|
| MCU | ESP32-C6 |
| LED | WS2812 RGB (GPIO 8) |

### Production Target
| Component | Part |
|-----------|------|
| AI Main Board | 立创·庐山派 K230-CanMV (1G) |
| WiFi/BLE MCU | ESP32-S3-WROOM-1-N16R8 |
| LCD Display | 1.9" TFT 170x320 ISP |
| Camera | GC2093 |
| Microphone | INMP441 (I2S, omnidirectional MEMS) |
| Speaker | 4Ω 2W |
| Amplifier | MAX98357A (I2S) |
| Servo | MG90S Metal Servo (180°) |
| Battery | 3.7V 3000mAh LiPo |
| Boost Converter | 5V 3A |
| Charging IC | IP5306 |
| Storage | 32GB Industrial TF Card |

---

## System Architecture

```
K230-CanMV (AI Brain)          ESP32-S3 (Control Hub)
──────────────────────         ───────────────────────────
Camera (GC2093)                INMP441 Mic     (I2S in)
Wake word detection      UART  MAX98357A Amp   (I2S out)
Face/emotion recognition ◄───► 1.9" LCD Display
Local AI inference             MG90S Servo
MicroPython runtime            TF Card
                               WiFi → Cloud AI API
                               WS2812 LED
```

### AI Voice Pipeline
```
User speaks
  → INMP441 (I2S) → ESP32-S3 records audio
  → WiFi → Cloud STT (Speech-to-Text)
  → Claude API (conversation)
  → Cloud TTS (Text-to-Speech)
  → MAX98357A → Speaker
  → ESP32-S3 drives LED + Servo (emotion expression)
```

---

## Features

- [x] WS2812 LED control
- [x] WiFi connection (Station mode)
- [x] HTTP server (LED on/off via REST)
- [ ] I2S audio recording (INMP441)
- [ ] I2S audio playback (MAX98357A)
- [ ] Cloud STT integration
- [ ] Claude AI conversation
- [ ] Cloud TTS integration
- [ ] LCD display driver
- [ ] Servo emotion expressions
- [ ] Wake word detection (K230)
- [ ] Camera emotion recognition (K230)
- [ ] K230 ↔ ESP32-S3 UART protocol
- [ ] Memory / conversation history

---

## Development

### Prerequisites
- ESP-IDF v5.x
- Python 3.8+
- VS Code + ESP-IDF extension

### Build & Flash
```bash
cd firmware/wifi_demo
idf.py build
idf.py flash monitor
```

### Run Tests
```bash
# Host-based unit tests (no hardware needed)
idf.py -T tests/unit build

# On-target integration tests
idf.py -T tests/integration flash monitor
```

---

## Project Structure
```
firmware/
├── blinky/          # LED blink demo
├── wifi_demo/       # WiFi + HTTP server + LED (modular components)
│   ├── components/
│   │   ├── bsp/           # Board Support Package
│   │   ├── led_driver/    # WS2812 LED abstraction
│   │   ├── wifi_manager/  # WiFi station management
│   │   └── http_server/   # REST API endpoints
│   └── main/
└── tests/           # Unity test suites (TDD)
    ├── unit/        # Host-based, no hardware
    └── integration/ # On-target hardware tests
```

---

## Development Approach

This project follows **TDD (Test-Driven Development)**:
1. Write a failing test first (Unity framework)
2. Write the minimum code to make it pass
3. Refactor

Each component in `firmware/wifi_demo/components/` has a corresponding test suite.

---

## Roadmap

| Phase | Focus | Board |
|-------|-------|-------|
| 1 | WiFi + LED + HTTP server | ESP32-C6 |
| 2 | I2S audio recording + playback | ESP32-C6 / S3 |
| 3 | Cloud STT + Claude API + TTS | ESP32-S3 |
| 4 | K230 wake word + camera | K230-CanMV |
| 5 | K230 ↔ ESP32-S3 integration | Both |
| 6 | Emotion system (LED + Servo + LCD) | ESP32-S3 |

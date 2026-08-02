# AI Companion Robot

A desktop AI companion robot with a **dual-brain architecture**: an ESP32-S3 handles
audio + body, a Kendryte K230 handles vision, and they talk to each other over a
3-wire UART link. It listens, chats (in English), remembers you, shows expressions,
and turns its head to follow your face.

Built as a portfolio project — embedded firmware (C / ESP-IDF), on-device vision
(MicroPython / CanMV), cloud AI (OpenAI), plus CI/CD and an OTA update pipeline.

---

## What it does

- 🎤 **Wake word** ("Hi ESP") → continuous English voice conversation
- 🧠 **Long-term memory** — remembers facts about you across reboots (stored in NVS)
- 😊 **Pixel-art emotions** on an ST7789 screen, driven by a state machine
- 👁️ **Face tracking** — K230 detects your face, the servo turns the head to follow you
- 🔌 **Two chips, one robot** — connected by just UART TX/RX + GND

---

## Architecture

```
        ESP32-S3  (audio brain + body)            K230  (vision brain)
   ┌────────────────────────────────┐        ┌──────────────────────────┐
   │ INMP441 mic (I2S)               │        │ GC2093 camera (CSI)      │
   │ Wake word (esp-sr AFE)          │  UART  │ KPU face detection       │
   │ STT → GPT → memory  (OpenAI)    │◄══════►│ compute face position    │
   │ ST7789 emotions / servo / LED   │  JSON  │ send events (NDJSON)     │
   └────────────────────────────────┘        └──────────────────────────┘
        WiFi → OpenAI (STT / GPT / TTS)
```

**Principle:** audio lives only on the ESP32, video only on the K230. The UART link
carries only small JSON events (e.g. `{"t":"face","x":0.3}`) — never audio or video.

### Voice pipeline
```
"Hi ESP" → record (VAD) → Whisper STT → GPT-4o-mini (streaming) → reply
                                                    └→ extract [remember] → NVS
```

### Vision pipeline
```
camera → KPU face detection → normalized face_x ∈ [-1,1] → UART → servo follows
```

---

## Hardware

| Component | Part | Where |
|-----------|------|-------|
| Audio/body MCU | ESP32-S3-WROOM-1 (N16R8) | main brain |
| Vision SoC | Kendryte K230 (CanMV) | camera brain |
| Microphone | INMP441 (I2S MEMS) | ESP32 |
| Display | ST7789 1.9" 320×170 SPI | ESP32 |
| Servo | continuous-rotation (head) | ESP32 |
| LED | WS2812 | ESP32 |
| Speaker | MAX98357A + 4Ω (planned) | ESP32 |
| Camera | GC2093 | K230 |

Full pin map: [`esp32/components/bsp/bsp_board.h`](esp32/components/bsp/bsp_board.h) ·
wiring notes: [`docs/wiring.md`](docs/wiring.md)

---

## Software

```
esp32/                 ESP-IDF firmware (C)
├── main/              integrated app (state machine + all subsystems)
├── components/        bsp, stt, llm, memory, emotion_engine, servo_driver,
│                      uart_bridge, led_driver, display_driver, app_state
└── examples/          per-feature test apps (mic, screen, servo, uart, face_track…)
k230/                  CanMV MicroPython (vision brain)
├── main.py            boot auto-run: face detection → UART
└── ai_robot/          uart_link, board, face_detect
shared/protocol/       UART NDJSON protocol spec
tests/                 host unit tests (pytest)
docs/                  wiring, dev log, TODO
```

---

## Build & run

### ESP32 (ESP-IDF)
```bash
cd esp32
cp main/config.h.example main/config.h    # then fill in WiFi + OpenAI key
idf.py set-target esp32s3
idf.py menuconfig                          # ESP Speech Recognition → Hi ESP
idf.py -p <PORT> flash monitor
```

### K230 (CanMV)
Copy `k230/main.py` to the SD card root (`/sdcard/main.py`) and `k230/ai_robot/*` to
`/sdcard/ai_robot/`. Power on (without the IDE) → face tracking runs automatically.

### Tests
```bash
pip install pytest && pytest tests/ -v
```

---

## CI/CD

- **CI** (`.github/workflows/ci.yml`) — builds the ESP32 firmware and runs Python
  unit tests on every push.
- **Release** (`.github/workflows/release.yml`) — tag `vX.Y.Z` → builds and publishes
  the firmware `.bin` + `version.json` to GitHub Releases (source for OTA).

---

## Status

**Working:** wake word · English voice chat · long-term memory · pixel emotions ·
servo head · K230 face detection · face-following · full integrated firmware.

**Next:** Android/BLE companion app · WiFi provisioning · OTA updates · streaming TTS
(speaker) · custom PCB. See [`ROADMAP.md`](ROADMAP.md) and [`docs/TODO.md`](docs/TODO.md).

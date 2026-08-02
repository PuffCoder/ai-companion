# Roadmap — AI Companion Robot

> Development guide and design-decision record. Last updated: 2026-06.

---

## 1. Architecture (dual-brain)

Two chips, each doing what it's best at. **Data stays local — never shuffled between them.**

```
      ESP32-S3 (audio brain + body)            K230 (vision brain)
   ┌────────────────────────────────┐      ┌──────────────────────────┐
   │ INMP441 mic (I2S)               │      │ GC2093 camera            │
   │ Wake word (esp-sr)              │ UART │ KPU: face detection      │
   │ WiFi → OpenAI voice pipeline    │◄════►│ (recognition, emotion…)  │
   │ STT → GPT → TTS                 │ JSON │ local inference (offline)│
   │ WS2812 LED / servo / LCD        │      │ vision events → UART     │
   └────────────────────────────────┘      └──────────────────────────┘
```

- **ESP32 = audio brain + body**: mic + speaker + AI voice + LED/servo/screen + the LLM/memory/persona.
- **K230 = vision brain**: camera + KPU, runs detection locally, sends results over UART.
- Audio only on ESP32, video only on K230 — **true parallelism**, no resource contention.
- UART carries only small JSON events; audio/video never cross it.

**Why split this way:** solves the camera + audio resource contention (listen and see at
the same time); K230 vision is local KPU inference (works offline); each chip keeps its
data local instead of pushing big data over a slow UART.

---

## 2. Tech choices

| Area | Choice | Why |
|------|--------|-----|
| ESP32 framework | ESP-IDF + FreeRTOS | C, component architecture |
| K230 | CanMV (MicroPython) | KPU vision, fast iteration |
| AI service | OpenAI (Whisper + GPT-4o-mini + TTS) | one key, direct HTTPS |
| Vision models | K230 KPU kmodels (face, yolov8, pose) | local inference |
| Board link | UART + NDJSON | small, robust, easy to debug |
| Quality | modular components, unit tests, CI/CD | portfolio-grade engineering |

---

## 3. Status

### Done ✅
- Voice: wake word ("Hi ESP") → English chat (STT → GPT streaming → reply)
- Long-term memory (NVS), persona system
- ST7789 pixel-art emotions, driven by app_state machine
- Servo head control
- UART bridge K230 ↔ ESP32 (NDJSON, bidirectional, HW-verified)
- K230 face detection → face_x → UART → servo face-following (closed loop)
- Full integrated firmware in `esp32/main` (voice + emotion + face-follow + LED)
- CI/CD pipeline (build + unit tests + release-to-GitHub-Releases)

### In progress 🟡
- Reliability tuning (heap monitoring for long-run stability)

---

## 4. Next: product direction

The core robot works. Next is turning it into a real product: a phone app to
configure/control it, and a firmware-update pipeline.

### Companion app + BLE
```
Android app ──BLE──► ESP32 ──UART──► K230
                       ├─ control (persona, actions)
                       ├─ provisioning (WiFi / persona / API key → NVS)
                       ├─ ESP32 firmware OTA
                       └─ relay K230 .py/.kmodel updates over UART
```
> BLE is the app↔robot channel; WiFi stays the robot↔cloud channel for AI.

### Epics (tracked in Jira)
1. **Config to NVS** — make WiFi/API key runtime-settable (foundation)
2. **BLE control** — ESP32 GATT server + Android app
3. **BLE provisioning** — set WiFi / persona / key from the app
4. **ESP32 OTA over BLE** — dual-slot partitions, receive + apply + rollback
5. **K230 update relay** — push .py/.kmodel through ESP32 → K230 SD card
6. **Hardware & PCB** — speaker + streaming TTS, power design, custom PCB
7. **Product design & identity** — finalize wake word + appearance

### OTA / fleet updates
```
push code → CI builds → GitHub Releases (.bin + version.json)
                                   ↑
             device checks version.json → downloads → OTA → reboot
```
No custom backend needed to start — GitHub Releases hosts the firmware.

---

## 5. Known limits (design constraints)

| Limit | Reality | Impact |
|-------|---------|--------|
| Voice latency | serial HTTP calls, ~3–8 s floor | not instant; network-bound |
| Barge-in | single-threaded flow | strict turn-taking |
| Battery | two chips, high draw | fine for desktop use |
| Offline | voice needs internet; vision is local | face tracking works offline |
| TTS | speaker not wired yet | replies print to serial for now |

---

## 6. Recommended order

```
Config→NVS → BLE control → provisioning → OTA → K230 relay      (software product)
Speaker + streaming TTS → power → PCB schematic → layout → fab  (hardware product)
```
Run both tracks in parallel. Do speaker + TTS early — it makes the robot talk out loud.

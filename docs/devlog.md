# Dev Log

> AI Companion Robot. Milestones, gotchas, and fixes.

---

## 2026-06-08 — ESP32 voice pipeline (mic + STT + TTS)

### ✅ Done
- **INMP441 microphone** (I2S) working
- **STT** (OpenAI Whisper): record → WAV → upload → HTTP 200
- **TLS** to OpenAI working (cross-signed certificate verification)

### 🔧 Key gotchas & fixes

**1. Old ESP32 board had a dead GPIO42**
- Symptom: same wiring/code, old board read garbage, new board fine
- Fix: swap board. ESP32-S3 I2S can route to any free GPIO, so a bad pin is swappable

**2. INMP441 is a 24-bit mic**
- Symptom: reading with a 16-bit I2S slot gave a very weak signal (avg 8–100)
- Fix: read with a **32-bit I2S slot**, then `>> MIC_SHIFT` to scale down to 16-bit
- `slot_mask = I2S_STD_SLOT_LEFT` (L/R = GND = left channel)

**3. GND on the wrong side**
- Symptom: L/R tied to a left-side GND (under GPIO14) → all-zero / railing
- Fix: use a **right-side GND** (under GPIO19). See docs/wiring.md

**4. Intermittent contact (recurring)**
- Symptom: mostly-zero peaks, occasional 32768, flaky drops
- Fix: direct female-female dupont / solder, **ditch the breadboard**. Short, firm wires

**5. main task stack overflow**
- Symptom: `stack overflow in task main` while uploading to Whisper
- Fix: run record + HTTP + TLS in a **separate 8KB-stack task** (xTaskCreate)
- ⚠️ TTS overflows the same way (see TODO below)

**6. TLS certificate verification failed**
- Symptom: `No matching trusted root certificate found`
- Fix: enable in menuconfig
  - full certificate bundle (DEFAULT_FULL)
  - **Support cross-signed certificate verification** (key — DST Root CA X3)
- Proper security, not skipping verification

### ⚠️ In progress / TODO (at the time)
- **TTS stack overflow**: same cause as STT — HTTP + TLS + audio decode eats stack.
  Fix: run TTS in a big-stack task (≥8KB, give 12KB for TLS headroom).
- STT occasionally returns empty text: recording timing / volume — make sure the
  spoken segment is actually captured.
- Temporary insecure-TLS config (ESP_TLS_INSECURE) reverted to proper bundle verify.

### TTS done (after soldering)
- Soldered headers → mic recording stable (avg 1370, no drops)
- **STT works**: recognized "你好,今天天气怎么样?"; background noise didn't break it
- **TTS works**: text → OpenAI TTS → clear speech
- Wrapped `stt` component (stt_transcribe_pcm) + `mic` component
- Added tts_test example: text → TTS → send to PC as WAV to audition (no speaker needed)

### TTS transport gotcha
- First used UDP for TTS audio → dropped/out-of-order packets → corrupted audio
- Switched to **TCP** (PC as server, ESP connects and sends) → complete, ordered, clear
- Lesson: UDP for live monitoring (dropping a little is fine), TCP for whole-file transfer
- TTS uses `response_format=pcm` (raw PCM 24kHz mono), no file header
- TTS runs in a 12KB-stack task (avoids overflow)

### Wake word working (esp-sr WakeNet)
- **"Hi ESP" wake-word detection works**: one shout triggers it, fully local (offline)
- esp-sr AFE + WakeNet9 (wn9_hiesp), fed by INMP441 audio
- esp-sr available on IDF v6.0.1
- Gotcha: header is not esp_srmodel.h, it's **model_path.h**; component name **espressif__esp-sr**
- Key API: esp_srmodel_init("model") → afe_config_init("M",...) →
  esp_afe_handle_from_config → create_from_config → feed/fetch →
  res->wakeup_state == WAKENET_DETECTED
- Needs a custom partition table with a 4MB model partition + wake word selected in menuconfig
- Added llm component (GPT chat with multi-turn history)

### All ESP32 AI building blocks ready
✅ wake word (Hi ESP) ✅ record ✅ STT (Whisper) ✅ GPT chat ✅ TTS
Next: wire them into a full loop — chat_test (wake → record → STT → GPT → TTS)

### Full conversation loop working 🎉
- **chat_test**: "Hi ESP" → record → STT → GPT → reply printed to serial
- Wake word + mic share one I2S (no conflict)
- Whole flow in a 16KB-stack task (AFE + TLS)
- **Multi-turn memory verified**: "I like basketball" → later "what do I like" answers it
- No speaker yet, reply printed to serial; TTS playback slots in here later
- Gotcha: EXTRA_COMPONENT_DIRS pointing at the whole components/ compiles ALL components
  (wifi_manager etc. need Kconfig) → make examples self-contained by copying stt/llm in

### Conversation UX polish (chat_test)
- **VAD auto-stop**: 500ms of silence after you stop ends recording (no fixed 5s)
- **Continuous chat**: one "Hi ESP" then talk multiple turns, no re-waking each time
- **Exit**: exact whole-sentence match "再见/拜拜", or silence timeout (mid-sentence "再见" doesn't exit)
- **Volume gate** (VOICE_THRESHOLD=300): noise/silence isn't sent to Whisper, kills hallucinations
  (silence used to be transcribed as junk like "please like and subscribe")
- **Timing stats**: prints record / STT / GPT time each turn
- Measured: noise vol 139–185, speech 303–723; threshold 300 splits them well
- Performance: response (STT+GPT) ~7–8s, bottleneck is network + cloud, not the chip
  (K230 wouldn't be faster; TLS reuse helps little for intermittent chat)

### Long-term memory + persona
- Added **memory component**: stores user info + persona in NVS, survives power loss/reboot
- Persona templates (memory.h): Minion / Butler / Friend; default Minion, changeable in NVS
- System prompt = persona (NVS) + user info (NVS), loaded on boot
- Preloaded David's info (written on first boot, then persisted)
- Verified reload after reboot: "you are David, 28, engineer, likes skiing/motorcycles" + Minion persona
- Two kinds of memory: long-term (NVS, survives reboot) vs short-term (RAM history, cleared on reboot)
- Exit polish: 3 consecutive no-speech rounds auto-ends the chat (no more waiting forever)
- TODO: phase 2 switch persona by voice; phase 3 auto-extract facts into long-term memory

### Phase 3: auto-remember facts during conversation
- GPT tags remember-worthy info at the end of a reply with [记住]fact[/记住]
- Program parses the marker → stores to NVS (persists) → strips it from the reply (not shown)
- **Approach A**: on save, also llm_set_system() to refresh the live system prompt
  → remembered permanently without reboot, not pushed out by the short-term window (5 turns)
- memory_append_user() merges facts, with dedup (strstr) + cap protection
- Bug fix: llm s_system 256→1280 (was truncating the memory rule)
- Verified: "the dog is named 旺财", "music History" → still remembered 20+ turns later → and after reboot ✅
- Flashing firmware doesn't lose memory (NVS is a separate partition; only erase-flash clears it)

### GPT text streaming
- llm added llm_chat_stream(): request stream:true, HTTP callback parses SSE
  (line-by-line data:{json}, extract delta.content, stitch lines across ON_DATA)
- Callback prints chunks to serial; stops at '[' (hides the [记住] marker)
- Full reply still accumulated for history + memory-marker extraction
- Observation: no obvious difference on serial (GPT is fast, short replies appear almost instantly)
- Streaming's real value waits for the speaker: streaming TTS that plays as it generates
- TODO: retest streaming once the speaker (MAX98357A) arrives

### Verified working config
- Board: ESP32-S3-DEV-KIT-NXRX (N16R8, 16MB flash + 8MB PSRAM)
- INMP441: SCK=GPIO1, WS=GPIO2, SD=GPIO42, VDD=3V3, GND+L/R=right-side GND
- I2S: 32-bit slot, slot_mask LEFT, 16kHz mono, MIC_SHIFT ≈ 10–11
- Certs: full bundle + cross-signed verification
- Large buffers (audio/multipart) in PSRAM: heap_caps_malloc(MALLOC_CAP_SPIRAM)

---

## 2026-06-20 — UART bridge K230 ↔ ESP32 + K230 becomes the vision brain

### ✅ Done
- **UART bridge bidirectional, HW-verified** 🎉 — the two chips talk in JSON
  - ESP32: new `uart_bridge` component (UART1, GPIO17 TX / GPIO18 RX),
    NDJSON transport (one JSON per line, `\n`-terminated); `send_json()` + RX task line callback
  - example `examples/uart_bridge`: cJSON parse, ping/pong + face messages
  - K230: `ai_robot/uart_link.py` (`send(dict)` / `poll()`) + `tests/test_uart.py`
  - protocol doc: `shared/protocol/protocol.md` (face/noface/ping/pong/state)
  - Bring-up order: loopback self-test (TX↔RX shorted) → connect K230 both ways, step by step
- **K230 refactored to vision-only**: old voice modules (record/STT/GPT/TTS) archived to
  `legacy_audio/` (kept for reference, not deployed); `test_code/`→`reference/`; added `ai_robot/tests/`

### 🕳️ Gotchas
- **K230 UART pins = the Raspberry-Pi label trap**: physical pin8/pin10 are labeled
  GPIO14/15 on pinout.xyz (Pi diagram), but on **K230 they're actually GPIO3/GPIO4 = UART1**.
  Using the Pi numbers → `FPIOA set pin func failed`. Use the real K230 GPIO3/4.
  (K230: UART0/3 taken by DEBUG; usable are 1/2/4)
- `UART.UART0` attribute doesn't exist → don't eagerly evaluate all constants in a dict; use `getattr(UART,"UART1")`
- Use the FPIOA function constants via the instance `fpioa.UART1_TXD` (matches the official demo)
- example missing `main/idf_component.yml` declaring `espressif/cjson` → component not found
- `LINE_MAX` clashes with system `limits.h` → renamed `JSON_LINE_MAX`
- Running a script from the IDE has a cwd not in ai_robot → `os.chdir/sys.path` at the top to import
- Brief `dropped invalid JSON` at power-on (half-frames/noise), clean once stable — the protocol's
  "drop bad line and resync" working as intended, not a bug

### 🧭 Architecture decisions
- **Dropped BLDC**: no 2804 + AS5600 + PD6010B, a simple servo is enough for the head
- **K230 = vision only**, voice all on ESP32 (ESP32's voice pipeline is more complete)
- "3-wire neck (screen + camera both on K230)" = **future idea, not for the prototype**
  (too complex — needs an ST7789 MicroPython driver on K230 + separate head power)

### Verified wiring
```
ESP32 GPIO17(TX) → K230 pin10 GPIO4 (UART1 RXD)
ESP32 GPIO18(RX) ← K230 pin8  GPIO3 (UART1 TXD)
GND ──────────────  GND   (must share ground)
115200 8N1, direct 3.3V — no level shifter
```

---

## 2026-06-20 — English voice + ST7789 pixel-art emotions

### ✅ Voice switched to English (chat_test)
- Personas + default system prompt to English (memory.h personas, llm.c fallback)
- STT language `zh`→`en`; exit words `再见/拜拜`→`bye/goodbye` (`strcasecmp`, case-insensitive)
- Memory marker `[记住]`→`[remember]`; console logs all English
- Preloaded info (David) in English
- Gotcha: the longer English memory rule overflowed `sysprompt`/`s_system`
  (`format-truncation` error) → grew buffers to +512 / 1536
- **Must `erase-flash` after editing**: persona/user info live in NVS, old Chinese values
  aren't overwritten, so erase and re-preload

### ✅ ST7789 pixel-art emotions (screen_demo)
- New `emotion_engine` component: 7 expressions
  - IDLE blocky pixel eyes (retro-robot look) + the rest rounded eyes + eyebrows
    (happy/sad/angry/surprised/thinking/listening)
- Rounded corners = `fill_round` filling row by row (sqrt for the inset); eyebrows = thick slanted lines
- Smooth blink: eye height scaled by hpct; IDLE uses "eyelids closing" (draw full, then black top/bottom)

### 🕳️ Screen gotchas
- **Rotated wrong** → the panel is landscape: MADCTL `0x00`→`0xA0`, size 170x320→320x170, 35 offset moved from X to Y
- **4 vertical lines beside the eyes** → IDLE blocky eye (80px) wider than the clear box (76px), leaving 2 edge columns on switch → widened the clear box to 84
- **Visible flicker** → each frame "clear to black then draw row by row" has a black frame → moved to an
  **offscreen framebuffer**: render the whole eye in RAM, `display_blit` once (new driver API, single SPI DMA transfer)
- **IDLE blink not smooth** → was a two-state jump (open ↔ one line) → changed to eyelids closing, height shrinks continuously

### Components / interfaces
- `display_driver` added `display_blit(x,y,w,h,buf)`: blit a pre-rendered RGB565 buffer in one shot
- Screen 3.3V

---

## 2026-06-20 — Full integration + face-following + performance tuning

### ✅ Pin layout finalized + saved a GND pin
- Screen SPI moved to adjacent header pins GPIO 4/5/6/7/15/16 (module order SCL SDA RES DC CS BLK, plugs straight in)
- Speaker MAX98357A 41/40/39; removed the obsolete AS5600/BLDC; added a full pin-overview comment to BSP
- **Mic L/R now software-driven low on GPIO21** (saves a GND pin — 4 GNDs are enough: screen/servo/mic/K230)

### ✅ Face following (K230 vision → ESP32 head turn)
- K230 `face_detect.py`: official face_detection demo + compute face_x (largest face center, normalized -1..1) + UART send
- Camera `sensor_id=1` (CSI1); `display_size=None`; upside-down mount fixed with `pl.sensor.set_vflip(True)`
- ESP32 `examples/face_track`: continuous servo, proportional speed to follow, camera-feedback closed loop; `TRACK_SIGN` for direction
- **K230 `main.py` auto-run**: face detection on power-up (self-contained, uses uart_link/board), 3s window to interrupt via IDE

### ✅ Integrated firmware esp32/main
- Orchestrates: wake → chat + emotion screen + face-follow + LED, all driven by the `app_state` machine
- State→output mapping: IDLE blink/dim-green, LISTENING wide/blue, PROCESSING look-up/yellow, SPEAKING happy/magenta
- **Follows the face during conversation too** (keeps looking at you); no speaker yet, reply to serial
- emotion_engine promoted to a component; display_driver unified to landscape 320x170 + blit

### 🕳️ Integration gotchas
- **Running build inside main/** → "can't find `oleaut32`" host libs: main/CMakeLists is a component file, run from the `esp32/` root
- **IDF builds all components by default** → obsolete as5600 (BSP defines removed) + mic component errored → `set(COMPONENTS main)` to build only the dependency closure
- **Stutter while talking** → UART RX task priority 10 was starving the audio CPU → lowered to 4; throttled the K230 send rate
- **30fps needed for smoothness** → after the priority fix, put it back to ~30Hz (`SEND_MS=33`), no more audio stutter
- **`app_state` spamming IDLE→IDLE** → don't log if the state didn't change
- **Build config**: copied `partitions.csv` (esp-sr model partition) + `sdkconfig.defaults` (PSRAM/certs) from chat_test; added esp-sr to `main/idf_component.yml`

### 🔒 Security
- Found `mic_test/main/config.h` (with API key/WiFi) **was committed and pushed to GitHub** (commit 52c5da4)
- Handled: `git rm --cached` to stop tracking + added `config.h` to `.gitignore`; private repo so low risk, but advised rotating the OpenAI key (must rotate before ever making the repo public)

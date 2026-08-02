# TODO

> AI Companion Robot. Checked = done. Details in `devlog.md`, plan in `ROADMAP.md`.
> Detailed backlog with acceptance criteria lives in Jira.

---

## 🎯 Focus (next up)
- [ ] **Config to NVS** — move WiFi/API key from `config.h` to NVS (foundation for
  provisioning + OTA); no creds → provisioning mode, creds present → auto-connect
- [ ] **Streaming TTS + speaker** — wire MAX98357A, play OpenAI TTS out loud
  (replaces serial print)

## 📱 Companion app (BLE)
- [ ] ESP32 BLE GATT server + command protocol
- [ ] Android app: connect + send command (MVP)
- [ ] BLE provisioning (set WiFi / persona / API key from the app)

## 🔄 OTA firmware update
- [ ] Dual-slot partition table (ota_0 / ota_1)
- [ ] OTA client (version check / download / apply / rollback)
- [ ] K230 update relay (push .py/.kmodel through ESP32 → K230 SD)

## ⚙️ CI/CD
- [x] GitHub Actions build CI + release-to-Releases
- [x] Python host unit-test example (pytest)
- [ ] Unity host tests for ESP32 C logic (servo mapping, memory dedup)

## 🛠️ Hardware / design
- [ ] Power design (single input, 3.3V + 5V rails, current budget)
- [ ] Custom PCB (schematic → layout → fab → bring-up)
- [ ] 3D-printed shell
- [ ] Finalize wake word + appearance
- [ ] Rotate OpenAI API key (was committed to git history)

## 💡 Later / optional
- [ ] Face recognition (identify the user)
- [ ] Content-aware emotions (GPT emotion tags → screen)

---

## ✅ Done (recent)
- [x] Full integrated firmware in esp32/main (voice + emotion + face-follow + LED)
- [x] K230 face detection → face_x → UART → servo face-following
- [x] K230 boot auto-run (face tracking on power-up)
- [x] UART bridge K230 ↔ ESP32 (NDJSON, HW-verified)
- [x] ST7789 pixel-art emotions (flicker-free framebuffer)
- [x] English voice pipeline (wake word / STT / GPT streaming / memory / persona)
- [x] Continuous-rotation servo control

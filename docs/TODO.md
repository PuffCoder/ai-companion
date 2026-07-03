# TODO / 待办清单

> AI 小黄人陪伴机器人。勾掉 = 已完成。详细记录见 `devlog.md`，路线见 `ROADMAP.md`。
> 最后更新：2026-06-20

---

## 🎯 下一步（当前焦点）

- [ ] **人脸检测 (face detection)** — K230
  摄像头(GC2093) → KPU 检测人脸 → 算归一化 `face_x ∈ [-1,1]` →
  `uart_link.send({"t":"face","x":..,"n":..})` → ESP32 转头闭环。
  参考 `reference/object_detect_yolov8n_1.py`（检测流程）、`reference/gc2093.py`（摄像头）。
  不依赖屏，可立即做。

---

## K230（视觉脑）

- [ ] **ST7789 驱动 (k230-st7789 driver)**
  MicroPython 版 ST7789 SPI 驱动（找现成 / 适配 CanMV）。
  屏放 K230 属"3 线脖子"未来想法，prototype 阶段暂缓，但驱动可先备着。
- [ ] **表情动画模板 (motion template on st7789)**
  在 ST7789 上画机器人的"脸"：眨眼 / 喜怒等表情模板。
  依赖 ST7789 驱动；情绪由 ESP32 决定，UART 发 `{"t":"emotion","v":".."}` → K230 渲染。

## ESP32（音频脑 + 身体）

- [ ] **OTA 升级 (OTA update function)**
  WiFi 在线更新固件，免插线烧录。需 OTA 分区表 + esp_https_ota。
- [ ] **用户录入数据 (user enter data)**
  让用户自己输入个人信息（姓名/喜好）写入长期记忆，替代代码里硬编码。
  方式待定：WiFi 配网页面 / BLE / 串口命令。关联 memory + persona 组件。

## 机械 / 外观

- [ ] **3D 打印外壳 (3d print shell)**
  机器人本体外壳；要给舵机转头、摄像头、屏、麦克/扬声器、走线预留位置。

---

## ✅ 已完成（近期）
- [x] UART 桥 K230↔ESP32 双向通（NDJSON，HW verified）— 2026-06-20
- [x] K230 重构为视觉脑，旧语音归档 — 2026-06-20
- [x] 连续旋转舵机正反转/停 — 2026-06-19
- [x] 语音链路（唤醒词 / STT / GPT 流式 / TTS / 长期记忆 / 人设）

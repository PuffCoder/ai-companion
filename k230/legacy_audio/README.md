# legacy_audio — 已归档（语音模块）

这些是 K230 最早作为「语音 AI 原型」时的代码：录音 → STT → GPT → TTS。

## 为什么归档

架构已调整为**双脑分工**：

- **ESP32-S3** = 音频脑：麦克风、唤醒词、STT/TTS/GPT、长期记忆、人设、扬声器
- **K230** = 视觉脑：摄像头、人脸检测/识别、表情，结果经 UART 发给 ESP32

语音整块迁到 ESP32（它的语音链路更完整），所以 K230 这部分**退役**。

## 保留它干嘛

- 参考：STT/TTS/GPT 的 HTTP 调用写法（MicroPython 版）
- 对照：和 ESP32 的实现做对比

## 注意

- **不部署**到 `/sdcard/ai_robot/`，不是产品代码
- 这些文件 `from config import ...`、`from board import ...` 依赖原 `ai_robot/` 目录，
  直接在这里跑会 ImportError；要跑得自己补 `sys.path`
- `config.py` 仍在 `ai_robot/`（含凭证，勿动）

文件：`main_voice.py`(旧主程序) / `recorder.py` / `speech.py` / `llm.py` / `test_stt.py`

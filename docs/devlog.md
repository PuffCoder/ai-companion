# 开发日志 / Dev Log

> AI 小黄人陪伴机器人。记录里程碑、踩坑、解决方案。

---

## 2026-06-08 — ESP32 语音链路（麦克风 + STT + TTS）

### ✅ 已完成
- **INMP441 麦克风**（I2S）跑通
- **STT**（OpenAI Whisper）链路打通：录音 → WAV → 上传 → 返回 200
- **TLS** 到 OpenAI 成功（cross-signed 证书验证）

### 🔧 关键踩坑与解决

**1. 旧 ESP32 板 GPIO42 损坏**
- 现象：同样接线/代码，旧板读垃圾数据，新板正常
- 解决：换板。ESP32-S3 I2S 可路由任意空闲 GPIO，坏脚可换

**2. INMP441 是 24 位麦克风**
- 现象：用 16 位 I2S 槽读，信号极弱（avg 8~100）
- 解决：用 **32 位 I2S 槽**读取，再 `>> MIC_SHIFT` 缩放为 16 位
- `slot_mask = I2S_STD_SLOT_LEFT`（L/R 接 GND = 左声道）

**3. GND 接错侧**
- 现象：L/R 接左侧 GND（GPIO14 下方）→ 全 0 / railing
- 解决：改接**右侧 GND**（GPIO19 下方）。见 docs/wiring.md

**4. 接触不良（反复出现）**
- 现象：峰值大部分 0、偶尔 32768，间歇性断
- 解决：母对母杜邦线直连 / 焊接，**甩掉面包板**。线短插牢

**5. main 任务栈溢出**
- 现象：上传 Whisper 时 `stack overflow in task main`
- 解决：录音+HTTP+TLS 放进 **8KB 栈的独立任务**（xTaskCreate）
- ⚠️ TTS 同理会溢出（见下方待办）

**6. TLS 证书验证失败**
- 现象：`No matching trusted root certificate found`
- 解决：menuconfig 开启
  - 完整证书 bundle（DEFAULT_FULL）
  - **Support cross-signed certificate verification**（关键，DST Root CA X3）
- 这是正规安全方案，非跳过验证

### ⚠️ 进行中 / 待办
- **TTS 栈溢出**：和 STT 同因——TTS 的 HTTP+TLS+音频解码吃栈。
  解决：把 TTS 流程放进大栈任务（≥8KB，TLS 多可给 12KB）。
- STT 偶尔返回空文字：录音时机/音量，需保证说话时段被录到。
- TLS 临时跳过验证的配置（ESP_TLS_INSECURE）已改回正规 bundle 验证。

### TTS 完成（焊接后）
- 焊好排针 → 麦克风录音稳定（avg 1370，无丢包）
- **STT 成功**：识别 "你好,今天天气怎么样?"，杂音不影响识别
- **TTS 成功**：文字 → OpenAI TTS → 清晰语音
- 封装 `stt` 组件（stt_transcribe_pcm）+ `mic` 组件
- 新增 tts_test 例子：文字 → TTS → 传电脑存 WAV 试听（无需扬声器）

### TTS 传输踩坑
- 先用 UDP 传 TTS 音频 → 丢包/乱序 → 音频损坏听不出
- 改用 **TCP**（PC 当服务器，ESP 连接发送）→ 完整有序，清晰
- 教训：实时监听用 UDP（丢一点无所谓），完整文件传输用 TCP
- TTS 用 `response_format=pcm`（裸 PCM 24kHz 单声道），无文件头
- TTS 在 12KB 栈任务里跑（避免栈溢出）

### 唤醒词成功（esp-sr WakeNet）
- **"Hi ESP" 唤醒词检测跑通**：喊一声即触发，本地不联网
- 用 esp-sr 的 AFE + WakeNet9（wn9_hiesp），喂 INMP441 音频
- esp-sr 在 IDF v6.0.1 可用
- 踩坑：头文件不是 esp_srmodel.h，是 **model_path.h**；组件名 **espressif__esp-sr**
- 关键 API：esp_srmodel_init("model") → afe_config_init("M",...) →
  esp_afe_handle_from_config → create_from_config → feed/fetch →
  res->wakeup_state == WAKENET_DETECTED
- 需要自定义分区表 model 分区（4MB）+ menuconfig 选唤醒词
- 新增 llm 组件（GPT 对话，多轮历史）

### ESP32 端 AI 积木全齐
✅ 唤醒词(Hi ESP) ✅ 录音 ✅ STT(Whisper) ✅ GPT对话 ✅ TTS
下一步：串成完整闭环 chat_test（喊醒→录音→STT→GPT→TTS）

### 完整对话闭环跑通 🎉
- **chat_test**：喊"Hi ESP" → 录音 → STT → GPT → 串口打印回复
- 唤醒词 + 麦克风共用一个 I2S（不冲突）
- 整流程在 16KB 栈任务（AFE + TLS）
- **多轮记忆验证通过**：说"我喜欢打篮球" → 后面问"我喜欢什么"能答出来
- 无扬声器，回复打印串口；以后这里接 TTS 播放即可
- 踩坑：EXTRA_COMPONENT_DIRS 指向整个 components/ 会编译所有组件
  （wifi_manager 等需要 Kconfig）→ 改为把 stt/llm 复制进 example 自包含

### 对话体验优化（chat_test）
- **VAD 自动停录**：说完停嘴 500ms 自动结束，不再固定 5 秒
- **连续对话**：喊一次 "Hi ESP" 后可连说多句，无需每句唤醒
- **退出**：精确匹配"再见/拜拜"整句，或静音超时（句中含"再见"不误退）
- **音量门限**（VOICE_THRESHOLD=300）：噪声/静音不发 Whisper，消除幻觉
  （之前静音会被识别成"请点赞订阅""字幕由Amara提供"等垃圾）
- **耗时统计**：每轮打印 录音/STT/GPT 时间
- 实测：噪声音量 139-185，说话 303-723，门限 300 卡得好
- 性能：响应（STT+GPT）约 7-8 秒，瓶颈是网络+云端，非芯片
  （换 K230 不会更快；TLS 复用对间歇对话收益小）

### 长期记忆 + 人设
- 新增 **memory 组件**：NVS 存用户信息 + 人设，断电/重启不丢
- 人设模板库（memory.h）：小黄人/管家/朋友，默认小黄人，存 NVS 可改
- 系统提示 = 人设(NVS) + 用户信息(NVS)，开机加载
- 预填 David 信息（首次开机写入，之后持久化）
- 实测重启后正确加载："你是David，28岁，工程师，喜欢滑雪摩托" + 小黄人人设
- 区分两种记忆：长期(NVS，重启保留) vs 短期(RAM对话历史，重启清空)
- 退出优化：连续 3 次没有效说话自动结束对话（不再傻等）
- 待做：阶段2 语音切换人设；阶段3 对话中自动提取事实存长期记忆

### 阶段3：对话中自动记长期记忆
- GPT 在回复末尾用 [记住]事实[/记住] 标记值得长期记的信息
- 程序解析标记 → 存 NVS（重启保留）→ 从回复删除标记（不显示）
- **A 方案**：存的同时 llm_set_system() 刷新当前会话系统提示
  → 不重启也永久记住，且不被短期历史窗口（5轮）挤掉
- memory_append_user() 合并事实，含去重（strstr 查重复）+ 上限保护
- 修 bug：llm 的 s_system 256→1280（之前会截断记忆规则）
- 实测：说"小狗叫旺财""音乐History" → 聊20+轮仍记得 → 重启后仍记得 ✅
- 烧固件不丢记忆（NVS 与 app 分区分开；只有 erase-flash 才清）

### GPT 文字流式
- llm 加 llm_chat_stream()：请求 stream:true，HTTP 回调解析 SSE
  （逐行 data:{json}，提取 delta.content，跨 ON_DATA 拼行）
- 回调逐块 printf 到串口；遇 '[' 停打印（不显示 [记住] 标记）
- 完整回复仍累积，供存历史 + 提取记忆标记
- 现象：串口(COM)上看不出明显区别（GPT 很快，短回复几乎瞬间出全）
- 流式真正价值要等扬声器：流式 TTS 边生成边播才是体验质变
- 待办：扬声器（MAX98357A）到货后再测流式效果

### 已验证的工作配置
- 板：ESP32-S3-DEV-KIT-NXRX（N16R8，16MB flash + 8MB PSRAM）
- INMP441：SCK=GPIO1, WS=GPIO2, SD=GPIO42, VDD=3V3, GND+L/R=右侧 GND
- I2S：32 位槽，slot_mask LEFT，16kHz 单声道，MIC_SHIFT≈10~11
- 证书：完整 bundle + cross-signed 验证
- 大缓冲（音频/multipart）用 PSRAM：heap_caps_malloc(MALLOC_CAP_SPIRAM)

---

## 2026-06-20 — K230 ↔ ESP32 UART 桥打通 + K230 改造视觉脑

### ✅ 已完成
- **UART 桥双向实测通过** 🎉 —— 两颗芯片用 JSON 对话
  - ESP32：新增 `uart_bridge` 组件（UART1，GPIO17 TX / GPIO18 RX），
    NDJSON 传输（一行一条 JSON，`\n` 结尾）；`send_json()` + RX 任务按行回调
  - example `examples/uart_bridge`：cJSON 解析，ping/pong + face 消息
  - K230：`ai_robot/uart_link.py`（`send(dict)` / `poll()`）+ `tests/test_uart.py`
  - 协议文档：`shared/protocol/protocol.md`（face/noface/ping/pong/state）
  - 联调顺序：环回自测(TX↔RX 短接)→ 接 K230 双向，逐步验证
- **K230 重构为视觉脑**：旧语音模块(record/STT/GPT/TTS)归档到 `legacy_audio/`（保留参考，不部署）；`test_code/`→`reference/`；新增 `ai_robot/tests/`

### 🕳️ 踩坑
- **K230 UART 引脚 = 树莓派标签的坑**：物理 pin8/pin10 在 pinout.xyz(树莓派图)
  标 GPIO14/15，但 **K230 实际是 GPIO3/GPIO4 = 串口1(UART1)**。
  填树莓派号 → `FPIOA set pin func failed`。改成 K230 真实 GPIO3/4 才对。
  （庐山派：UART0/3 被 DEBUG 占，可用 1/2/4）
- `UART.UART0` 属性不存在 → 别在 dict 里预求值全部常量；按需 `getattr(UART,"UART1")`
- FPIOA 功能常量用实例 `fpioa.UART1_TXD`（与官方 demo 一致）
- example 缺 `main/idf_component.yml` 声明 `espressif/cjson` → 组件找不到
- `LINE_MAX` 与系统 `limits.h` 撞名 → 改 `JSON_LINE_MAX`
- IDE 直接运行脚本工作目录不在 ai_robot → 开头 `os.chdir/sys.path` 才能 import
- 上电瞬间有 `丢弃非法 JSON`（半截帧/噪声），稳定后干净 —— 协议"坏行丢弃重对齐"按预期工作，非 bug

### 🧭 架构决策
- **放弃 BLDC**：不用 2804+AS5600+PD6010B，头部用简单舵机即可
- **K230 = 纯视觉**，语音全归 ESP32（ESP32 语音链路更完整）
- "头/躯干 3 线脖子(屏+摄像头都上 K230)"= **未来想法，prototype 阶段不做**（太复杂，需 K230 写 ST7789 驱动 + 头部独立供电）

### 已验证的接线
```
ESP32 GPIO17(TX) → K230 pin10 GPIO4 (UART1 RXD)
ESP32 GPIO18(RX) ← K230 pin8  GPIO3 (UART1 TXD)
GND ──────────────  GND   （必须共地）
115200 8N1，3.3V 直连无需电平转换
```

---

## 2026-06-20 — 语音英文化 + ST7789 像素风表情

### ✅ 语音改英文（chat_test）
- 人设 + 默认系统提示改英文（memory.h personas、llm.c 兜底）
- STT 语言 `zh`→`en`；退出词 `再见/拜拜`→`bye/goodbye`（`strcasecmp` 大小写不敏感）
- 记忆标记 `[记住]`→`[remember]`；监视器日志全英文
- 预填信息（David）英文化
- 坑：英文记忆规则更长 → `sysprompt`/`s_system` 缓冲溢出（`format-truncation` 报错）→ 扩容到 +512 / 1536
- **改完必须 `erase-flash`**：人设/用户信息存在 NVS，旧中文不覆盖，要擦掉重新预填

### ✅ ST7789 像素风表情（screen_demo）
- 新增 `emotion_engine` 组件：7 个表情
  - IDLE 方块像素眼（复古机器人风）+ 其余圆角眼 + 眉毛（happy/sad/angry/surprised/thinking/listening）
- 圆角 = `fill_round` 逐行填充（sqrt 算内缩）；眉毛 = 粗斜线（角度定情绪）
- 平滑眨眼：眼高按 hpct 缩放；IDLE 用"眼皮渐合"（画满后黑色上下盖住）

### 🕳️ 屏踩坑
- **横纵反了** → 屏是横屏：MADCTL `0x00`→`0xA0`，尺寸 170x320→320x170，35 偏移从 X 挪到 Y
- **眼睛边上 4 条竖线** → IDLE 方块眼 80px 宽 > 清除区 76px，切表情时残留边缘两列 → 清除区加宽到 84
- **肉眼可见频闪** → 每帧"先刷黑再逐行画"有黑帧 → 改**离屏缓冲**：内存里画好整只眼，`display_blit` 一次刷出（新加驱动接口，单次 SPI DMA 传输）
- **IDLE 眨眼不丝滑** → 原来是"睁眼↔一横"两段跳 → 改成眼皮渐合，可见高度连续收缩

### 组件/接口
- `display_driver` 新增 `display_blit(x,y,w,h,buf)`：预渲染 RGB565 缓冲一次性刷屏
- 屏 3.3V；接线 SCLK12/MOSI11/CS10/DC9/RST8/BLK46

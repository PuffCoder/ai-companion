# board.py — 庐山派 K230 硬件引脚定义（HAL）
#
# ⚠️ 引脚以「庐山派官方原理图」为准，不要用树莓派 pinout 标签！
#    40 针排针机械兼容树莓派，但信号是 K230 自己映射的，与树莓派不同。
#
# ──────────────────────────────────────────────────────────────
# INMP441 麦克风接线（I2S，板上称 IIS）
# ──────────────────────────────────────────────────────────────
#   INMP441    40针位置    K230 信号        GPIO
#   ───────    ────────    ──────────       ─────
#   VDD    →   Pin 1       3V3              -       ⚠️ 勿接 5V，会烧片
#   GND    →   Pin 6       GND              -
#   SCK    →   Pin 37      IIS_CLK          GPIO32   位时钟 (BCLK)
#   WS     →   Pin 40      IIS_WS           GPIO33   字选择 (LRCLK)
#   SD     →   Pin 38      IIS_D_IN0        GPIO34   数据输入
#   L/R    →   GND         (选左声道)
#
# 物理针位：左排奇数 1,3,5...39；右排偶数 2,4,6...40（Pin1 = 方形焊盘）
# ──────────────────────────────────────────────────────────────

# ── I2S 输入（INMP441 麦克风）─────────────────────────────────
I2S_SCK = 32   # IIS_CLK   (Pin 37) 位时钟 BCLK
I2S_WS  = 33   # IIS_WS    (Pin 40) 字选择 LRCLK
I2S_SD  = 34   # IIS_D_IN0 (Pin 38) 数据输入

# ── I2S 输出（预留：接 MAX98357A 功放时用）────────────────────
I2S_DOUT = 35  # IIS_D_OUT0 (Pin 36) 数据输出

# ── 用户按键 ──────────────────────────────────────────────────
BTN_GPIO = 53  # PULL_DOWN，按下 = 1（与 config.py 一致）

# ── UART 桥（K230 ↔ ESP32）────────────────────────────────────
# ⚠️ 用 K230 自己的 GPIO 号，不是树莓派标签！物理 pin8/pin10 在树莓派图上
#    标 GPIO14/15，但 K230 实际是 GPIO3/GPIO4 = 串口1(UART1)。
#    （庐山派原理图：pin8=BANK0_GPIO3 TXD，pin10=BANK0_GPIO4 RXD）
#    K230 共5个串口，串口0/3 被 DEBUG 占，可用 1/2/4。
#
#   K230 pin8  GPIO3 (UART1 TXD) ──────→ ESP32 RX (GPIO18)
#   K230 pin10 GPIO4 (UART1 RXD) ←────── ESP32 TX (GPIO17)
#   GND                          ─────── GND   ← 必须共地
UART_TX   = 3     # K230 GPIO3，物理 pin8，UART1 TXD → ESP32 RX(18)
UART_RX   = 4     # K230 GPIO4，物理 pin10，UART1 RXD ← ESP32 TX(17)
UART_ID   = 1     # 串口1（UART0/3 被 DEBUG 占用）
UART_BAUD = 115200

# 注：CanMV 的 media.pyaudio 默认走板载音频通路。
#     外接 INMP441 是否需要显式 machine.I2S 配置，需接线后实测确认：
#     录音时轻敲 INMP441，看音量值是否跳变。

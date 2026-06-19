# test_uart.py — K230 ↔ ESP32 UART 桥联调测试（K230 端）
#
# 配合 ESP32 的 examples/uart_bridge 一起跑：
#   1) 两边接线：K230 TX→ESP32 RX(18)，K230 RX→ESP32 TX(17)，共 GND
#   2) ESP32 烧 examples/uart_bridge 并开 monitor
#   3) K230 跑本脚本
#
# 预期：
#   - ESP32 monitor 每 500ms 打印一条 "人脸: x=.. n=.."（收到 K230 的 face）
#   - ESP32 每秒发 ping，本脚本收到后回 pong，并打印 ping/pong
#   - 收发都通 = UART 桥 OK

import time, os, sys
# IDE 直接运行时工作目录不在 ai_robot，手动切过去才能 import 同目录模块
os.chdir("/sdcard/ai_robot")
sys.path.append("/sdcard/ai_robot")
from uart_link import UartLink

link = UartLink()

print("开始：每 500ms 发一条 face，收到 ping 回 pong")
x = -1.0
last_send = time.ticks_ms()

while True:
    # 收
    for msg in link.poll():
        t = msg.get("t")
        if t == "ping":
            print("收到 ping → 回 pong")
            link.send({"t": "pong"})
        elif t == "pong":
            print("收到 pong（链路通）")
        else:
            print("收到:", msg)

    # 每 500ms 发一条模拟人脸位置（x 在 -1..1 来回扫）
    now = time.ticks_ms()
    if time.ticks_diff(now, last_send) >= 500:
        link.send({"t": "face", "x": round(x, 2), "n": 1})
        x += 0.2
        if x > 1.0:
            x = -1.0
        last_send = now

    time.sleep_ms(10)

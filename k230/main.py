# main.py — K230 开机自动运行入口
#
# CanMV 上电（未连 IDE）时会自动执行 /sdcard/main.py。
# 本文件放 SD 卡根目录：/sdcard/main.py（不是 /sdcard/ai_robot/ 里面）。
# 它把工作目录切到 ai_robot 再跑主程序。
#
# 目前：UART 桥（发模拟 face + 回 pong），人脸检测接上后替换循环那段即可。

import os, sys, time

os.chdir("/sdcard/ai_robot")
sys.path.append("/sdcard/ai_robot")

# 开机留 3 秒缓冲：这段时间连上 IDE 可打断，方便改代码（否则死循环抢不回控制权）
print("K230 启动中... 9 秒内连 IDE 可打断")
time.sleep(9)

from uart_link import UartLink

def run():
    link = UartLink()
    print("K230 视觉脑就绪，UART 已连")

    x = -1.0
    last = time.ticks_ms()
    while True:
        # 收：回应 ESP32 的 ping
        for msg in link.poll():
            if msg.get("t") == "ping":
                link.send({"t": "pong"})

        # 发：每 500ms 一条 face（人脸检测接上后换成真实 face_x）
        now = time.ticks_ms()
        if time.ticks_diff(now, last) >= 500:
            link.send({"t": "face", "x": round(x, 2), "n": 1})
            x += 0.2
            if x > 1.0:
                x = -1.0
            last = now
        time.sleep_ms(10)

# 出错不让开机卡死，打印异常方便排查
try:
    run()
except Exception as e:
    print("main.py 异常:", e)

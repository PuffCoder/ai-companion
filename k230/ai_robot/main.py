import time, os, sys, network
from media.media import *

# 把 ai_robot 加入模块搜索路径
#sys.path.append("/sdcard/ai_robot")
os.chdir("/sdcard/ai_robot")

from config   import WIFI_SSID, WIFI_PASS
from button   import wait_press
from recorder import record
from speech   import transcribe, say
from llm      import chat

# ── WiFi 连接 ──────────────────────────────────────────────────
def connect_wifi():
    print("SSID:", WIFI_SSID)    # ← 加这行确认
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    time.sleep(1)                 # ← 加等待
    if not wlan.isconnected():
        print("连接 WiFi...")
        wlan.connect(WIFI_SSID, WIFI_PASS)
        for i in range(15):
            if wlan.isconnected():
                break
            time.sleep(1)
            print(".", end="")
    if wlan.isconnected():
        print("\nWiFi OK:", wlan.ifconfig()[0])
    else:
        print("\nWiFi 连接失败")

# ── 主程序 ─────────────────────────────────────────────────────
try:
    connect_wifi()
    MediaManager.init()          # 全局初始化一次
    print("\n=== 语音对话就绪 ===")
    print("按用户键开始说话，再按停止\n")

    while True:
        wait_press()             # 第一次按键 → 开始
        record()                 # 录音，第二次按键 → 停止
        text = transcribe()      # STT
        if text.strip():
            reply = chat(text)   # GPT
            say(reply)           # TTS + 播放

except KeyboardInterrupt:
    print("\n用户退出")
finally:
    try:
        MediaManager.deinit()
    except:
        pass
    os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
    time.sleep_ms(100)

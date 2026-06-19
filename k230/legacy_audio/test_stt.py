# STT 独立测试：按键录音 → 音量门限 → Whisper 识别
# 只测语音转文字，不走 GPT / TTS，方便聚焦调试。
import time, os, sys, network
from media.media import *

os.chdir("/sdcard/ai_robot")

from config   import WIFI_SSID, WIFI_PASS
from button   import wait_press
from recorder import record, has_voice
from speech   import transcribe

# ── WiFi ──────────────────────────────────────────────────────
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    if not wlan.isconnected():
        print("连接 WiFi...")
        wlan.connect(WIFI_SSID, WIFI_PASS)
        for _ in range(15):
            if wlan.isconnected():
                break
            time.sleep(1)
    if wlan.isconnected():
        print("WiFi OK:", wlan.ifconfig()[0])
    else:
        raise Exception("WiFi 连接失败")

# ── 主测试循环 ────────────────────────────────────────────────
try:
    connect_wifi()
    MediaManager.init()
    print("\n=== STT 测试 ===")
    print("按用户键开始说话，再按停止\n")

    while True:
        wait_press()
        record()

        # 关键：静音不发给 Whisper，避免幻觉
        if not has_voice():
            print("→ 跳过（未检测到语音）\n")
            continue

        text = transcribe()
        print("识别结果:", repr(text), "\n")

except KeyboardInterrupt:
    print("\n退出测试")
finally:
    try:
        MediaManager.deinit()
    except:
        pass
    os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
    time.sleep_ms(100)

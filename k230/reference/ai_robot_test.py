import requests, ujson, network, time
from media.pyaudio import *
import media.wave as wave
from media.media import *

API_KEY = "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"


# ── WiFi ─────────────────────────────────────────
def connect_wifi(ssid, password):
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        wlan.connect(ssid, password)
        while not wlan.isconnected():
            time.sleep(1)
    print("WiFi OK:", wlan.ifconfig()[0])

# ── GPT 对话 ──────────────────────────────────────
def chat(message):
    resp = requests.post(
        "https://api.openai.com/v1/chat/completions",
        headers={
            "Authorization": "Bearer " + API_KEY,
            "Content-Type": "application/json"
        },
        data=ujson.dumps({
            "model": "gpt-4o-mini",
            "max_tokens": 128,
            "messages": [{"role": "user", "content": message}]
        }).encode("utf-8")
    )
    data = ujson.loads(resp.text)
    resp.close()
    return data["choices"][0]["message"]["content"]

# ── TTS：文字 → WAV 文件 ──────────────────────────
def tts(text, path="/sdcard/tts.wav"):
    print("TTS 生成中...")
    resp = requests.post(
        "https://api.openai.com/v1/audio/speech",
        headers={
            "Authorization": "Bearer " + API_KEY,
            "Content-Type": "application/json"
        },
        data=ujson.dumps({
            "model": "tts-1",
            "input": text,
            "voice": "nova",           # 声音：alloy/nova/shimmer/echo
            "response_format": "wav"
        }).encode("utf-8")
    )
    with open(path, "wb") as f:
        f.write(resp.content)
    resp.close()
    print("TTS 保存:", path)

# ── 播放 WAV ──────────────────────────────────────
def play_wav(path="/sdcard/tts.wav"):
    print("读取音频...")
    with open(path, "rb") as f:
        f.read(44)           # 跳过 WAV 头
        audio_data = f.read() # 全部读进内存

    print("播放中...")
    p = PyAudio()
    stream = p.open(
        format=paInt16,
        channels=1,
        rate=24000,
        output=True,
        frames_per_buffer=8192  # 增大 buffer
    )
    MediaManager.init()
    stream.write(audio_data)    # ← 替换原来的 while 循环
    stream.stop_stream()

    # 从内存直接写入
    CHUNK = 8192
    offset = 0
    while offset < len(audio_data):
        stream.write(audio_data[offset:offset + CHUNK])
        offset += CHUNK

    stream.stop_stream()
    stream.close()
    p.terminate()
    MediaManager.deinit()
    print("播放完毕")
# ── 主程序 ────────────────────────────────────────
connect_wifi("遥遥领先", "0211016863")
reply = chat("你好！用一句话介绍自己。")
print("GPT:", reply)

tts(reply)
play_wav()

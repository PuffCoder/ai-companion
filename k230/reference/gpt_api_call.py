import time, os, network, requests, ujson

from machine import Pin
from media.sensor import *
from media.display import *
from media.media import *
from media.pyaudio import *

# ── 配置 ──────────────────────────────────────────
WIFI_SSID = "遥遥领先"
WIFI_PASS = "0211016863"
API_KEY = "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"
USER_BTN   = Pin(53, Pin.IN, Pin.PULL_DOWN)  # 用户按键 GPIO
history    = []



# ── WiFi ──────────────────────────────────────────
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    if not wlan.isconnected():
        wlan.connect(WIFI_SSID, WIFI_PASS)
        while not wlan.isconnected():
            time.sleep(1)
    print("WiFi OK:", wlan.ifconfig()[0])

# ── WAV 头 ────────────────────────────────────────
def make_wav_header(data_size, rate=16000):
    h  = b'RIFF' + (36 + data_size).to_bytes(4,'little')
    h += b'WAVEfmt ' + (16).to_bytes(4,'little')
    h += (1).to_bytes(2,'little') + (1).to_bytes(2,'little')
    h += rate.to_bytes(4,'little') + (rate*2).to_bytes(4,'little')
    h += (2).to_bytes(2,'little') + (16).to_bytes(2,'little')
    h += b'data' + data_size.to_bytes(4,'little')
    return h

# ── 按键等待 ──────────────────────────────────────
def wait_btn_press():
    while USER_BTN.value() == 1:   # 等按下
        time.sleep_ms(10)
    time.sleep_ms(50)              # 消抖
    while USER_BTN.value() == 0:   # 等松开
        time.sleep_ms(10)

# ── 录音（按第二下停止）────────────────────────────
def record(path="/sdcard/rec.wav"):
    RATE, CHUNK = 16000, 4096
    p = PyAudio()
    stream = p.open(format=paInt16, channels=1,
                    rate=RATE, input=True,
                    frames_per_buffer=CHUNK)
    print("🎤 录音中... 再按按键停止")
    frames = []
    try:
        MediaManager.init()
        while USER_BTN.value() == 1:   # 松手/未按 = 继续录
            frames.append(stream.read(CHUNK))
    finally:
        stream.stop_stream()
        stream.close()
        p.terminate()
        try: MediaManager.deinit()
        except: pass

    pcm = b''.join(frames)
    with open(path, "wb") as f:
        f.write(make_wav_header(len(pcm)))
        f.write(pcm)
    print("录音完成")
    return path

# ── Whisper STT ───────────────────────────────────
def transcribe(path="/sdcard/rec.wav"):
    print("识别中...")
    with open(path, "rb") as f:
        audio = f.read()
    bd   = "Boundary7MA4YW"
    body = ("--"+bd+"\r\n").encode()
    body += b'Content-Disposition: form-data; name="model"\r\n\r\nwhisper-1\r\n'
    body += ("--"+bd+"\r\n").encode()
    body += b'Content-Disposition: form-data; name="language"\r\n\r\nzh\r\n'
    body += ("--"+bd+"\r\n").encode()
    body += b'Content-Disposition: form-data; name="file"; filename="rec.wav"\r\nContent-Type: audio/wav\r\n\r\n'
    body += audio + ("\r\n--"+bd+"--\r\n").encode()
    resp = requests.post(
        "https://api.openai.com/v1/audio/transcriptions",
        headers={"Authorization":"Bearer "+API_KEY,
                 "Content-Type":"multipart/form-data; boundary="+bd},
        data=body)
    text = ujson.loads(resp.text).get("text","")
    resp.close()
    print("你说:", text)
    return text

# ── GPT ───────────────────────────────────────────
def chat(msg):
    history.append({"role":"user","content":msg})
    resp = requests.post(
        "https://api.openai.com/v1/chat/completions",
        headers={"Authorization":"Bearer "+API_KEY,"Content-Type":"application/json"},
        data=ujson.dumps({
            "model":"gpt-4o-mini","max_tokens":150,
            "messages":[{"role":"system","content":"你是AI机器人小黄人，回答简短可爱。"}]+history
        }).encode("utf-8"))
    reply = ujson.loads(resp.text)["choices"][0]["message"]["content"]
    resp.close()
    history.append({"role":"assistant","content":reply})
    print("AI:", reply)
    return reply

# ── TTS 播放 ──────────────────────────────────────
#
#
def say(text):
    print("TTS 请求中...")
    resp = requests.post(
        "https://api.openai.com/v1/audio/speech",
        headers={"Authorization":"Bearer "+API_KEY,"Content-Type":"application/json"},
        data=ujson.dumps({"model":"tts-1","input":text,
                          "voice":"nova","response_format":"wav"}).encode("utf-8"))
    audio = resp.content[44:]
    resp.close()
    print("音频大小:", len(audio), "bytes")

    time.sleep_ms(500)   # 等录音资源完全释放

    p = PyAudio()
    try:
        print("打开播放流...")
        stream = p.open(format=paInt16, channels=1, rate=24000,
                        output=True, frames_per_buffer=16384)
        print("初始化 MediaManager...")
        MediaManager.init()
        print("开始播放...")
        stream.write(audio)
        print("播放完成 ✅")
        stream.stop_stream()
        stream.close()
    except Exception as e:
        print("播放错误:", e)   # 现在能看到真正的错误
    finally:
        p.terminate()
        try: MediaManager.deinit()
        except: pass


# ── 主程序 ────────────────────────────────────────
try:
    connect_wifi()
    print("=== 语音对话就绪 ===")
    print("按用户键开始说话，再按停止")

    while True:
        wait_btn_press()         # 第一次按 → 开始录音
        record()                 # 录到第二次按
        text = transcribe()
        if text.strip():
            reply = chat(text)
            say(reply)

except KeyboardInterrupt:
    print("用户退出")
finally:
    os.exitpoint(os.EXITPOINT_ENABLE_SLEEP)
    time.sleep_ms(100)

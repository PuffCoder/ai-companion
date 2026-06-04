import requests, ujson
from media.pyaudio import *
from config import API_KEY

def transcribe(path="/sdcard/rec.wav"):
    """语音转文字 (Whisper)"""
    print("识别中...")
    with open(path, "rb") as f:
        audio = f.read()

    bd   = "Bnd7MA4YW"
    body  = ("--" + bd + "\r\n").encode()
    body += b'Content-Disposition: form-data; name="model"\r\n\r\nwhisper-1\r\n'
    body += ("--" + bd + "\r\n").encode()
    body += b'Content-Disposition: form-data; name="language"\r\n\r\nzh\r\n'
    body += ("--" + bd + "\r\n").encode()
    body += b'Content-Disposition: form-data; name="file"; filename="rec.wav"\r\n'
    body += b'Content-Type: audio/wav\r\n\r\n'
    body += audio + ("\r\n--" + bd + "--\r\n").encode()

    resp = requests.post(
        "https://api.openai.com/v1/audio/transcriptions",
        headers={
            "Authorization": "Bearer " + API_KEY,
            "Content-Type": "multipart/form-data; boundary=" + bd
        },
        data=body
    )
    text = ujson.loads(resp.text).get("text", "")
    resp.close()
    print("你说:", text)
    return text

def say(text):
    """文字转语音并播放 (TTS)"""
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
            "voice": "nova",
            "response_format": "wav"
        }).encode("utf-8")
    )
    audio = resp.content[44:]   # 跳过 WAV 头
    resp.close()
    print("播放中...", len(audio), "bytes")

    p = PyAudio()
    stream = p.open(format=paInt16, channels=1,
                    rate=24000, output=True, frames_per_buffer=16384)
    stream.write(audio)
    stream.stop_stream()
    stream.close()
    p.terminate()
    print("播放完成 ✅")

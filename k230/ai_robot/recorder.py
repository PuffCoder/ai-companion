from media.pyaudio import *
from button import is_pressed
import time

RATE  = 16000
CHUNK = 4096

def _make_wav_header(n):
    h  = b'RIFF' + (36 + n).to_bytes(4, 'little')
    h += b'WAVEfmt ' + (16).to_bytes(4, 'little')
    h += (1).to_bytes(2, 'little') + (1).to_bytes(2, 'little')
    h += RATE.to_bytes(4, 'little') + (RATE * 2).to_bytes(4, 'little')
    h += (2).to_bytes(2, 'little') + (16).to_bytes(2, 'little')
    h += b'data' + n.to_bytes(4, 'little')
    return h

def record(path="/sdcard/rec.wav"):
    p = PyAudio()
    stream = p.open(format=paInt16, channels=1,
                    rate=RATE, input=True, frames_per_buffer=CHUNK)
    print("🎤 录音中... 再按停止")
    frames = []

    while not is_pressed():              # 录到第二次按键
        frames.append(stream.read(CHUNK))
    while is_pressed():                  # 等松开
        time.sleep_ms(10)

    stream.stop_stream()
    stream.close()
    p.terminate()

    pcm = b''.join(frames)
    with open(path, "wb") as f:
        f.write(_make_wav_header(len(pcm)))
        f.write(pcm)

    secs = len(pcm) // (RATE * 2)
    print(f"录音完成 ({secs}秒)")
    return path

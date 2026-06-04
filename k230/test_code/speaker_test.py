from media.pyaudio import *
from media.media import *

PATH = "/sdcard/song.wav"

# 读 WAV 头里的采样率和声道数
with open(PATH, "rb") as f:
    header = f.read(44)
    channels = int.from_bytes(header[22:24], 'little')
    rate     = int.from_bytes(header[24:28], 'little')
    audio    = f.read()

print(f"声道:{channels} 采样率:{rate}")

MediaManager.init()
p = PyAudio()
stream = p.open(format=paInt16, channels=channels,
                rate=rate, output=True, frames_per_buffer=16384)
try:
    stream.write(audio)
finally:
    stream.stop_stream(); stream.close(); p.terminate()
    try: MediaManager.deinit()
    except: pass
print("播放完成")

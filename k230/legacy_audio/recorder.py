# 录音模块。外接 INMP441 麦克风接线见 board.py。
# 现阶段用 media.pyaudio（板载/默认音频通路）；外接 I2S 麦待实测确认。
from media.pyaudio import *
from button import is_pressed
import time

RATE  = 16000
CHUNK = 4096

# 音量门限：录音平均振幅低于此值视为静音（16-bit 范围 0~32767）
VOICE_THRESHOLD = 300

# 软件增益：录音太小声时放大，让 Whisper 听清。1=不放大，4=放大4倍
# 太大会削波失真，从 4 起调
GAIN = 8

def _make_wav_header(n):
    h  = b'RIFF' + (36 + n).to_bytes(4, 'little')
    h += b'WAVEfmt ' + (16).to_bytes(4, 'little')
    h += (1).to_bytes(2, 'little') + (1).to_bytes(2, 'little')
    h += RATE.to_bytes(4, 'little') + (RATE * 2).to_bytes(4, 'little')
    h += (2).to_bytes(2, 'little') + (16).to_bytes(2, 'little')
    h += b'data' + n.to_bytes(4, 'little')
    return h

def _apply_gain(pcm, gain):
    """放大 PCM 音量。每个 16-bit 采样乘 gain，超范围削波。"""
    if gain == 1:
        return pcm
    out = bytearray(len(pcm))
    for i in range(0, len(pcm) - 1, 2):
        val = pcm[i] | (pcm[i + 1] << 8)
        if val >= 32768:
            val -= 65536
        val = int(val * gain)
        if val > 32767:
            val = 32767
        elif val < -32768:
            val = -32768
        if val < 0:
            val += 65536
        out[i]     = val & 0xFF
        out[i + 1] = (val >> 8) & 0xFF
    return bytes(out)

def _avg_amplitude(pcm, step=50):
    """计算 PCM 平均振幅。每 step 个采样抽一个，保证速度。"""
    total = 0
    count = 0
    for i in range(0, len(pcm) - 1, 2 * step):
        val = pcm[i] | (pcm[i + 1] << 8)   # 小端 16-bit
        if val >= 32768:                    # 转有符号
            val -= 65536
        total += abs(val)
        count += 1
    return total // count if count else 0

def has_voice(path="/sdcard/rec.wav", threshold=VOICE_THRESHOLD):
    """判断 WAV 是否含有效语音（音量是否超过门限）。"""
    with open(path, "rb") as f:
        f.read(44)              # 跳过 WAV 头
        pcm = f.read()
    amp = _avg_amplitude(pcm)
    voiced = amp >= threshold
    print(f"音量: {amp}  门限: {threshold}  -> {'有效' if voiced else '静音'}")
    return voiced

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
    pcm = _apply_gain(pcm, GAIN)          # 放大音量

    with open(path, "wb") as f:
        f.write(_make_wav_header(len(pcm)))
        f.write(pcm)

    secs = len(pcm) // (RATE * 2)
    print(f"录音完成 ({secs}秒, 增益x{GAIN})")
    return path

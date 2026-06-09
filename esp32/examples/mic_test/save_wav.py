"""
save_wav.py — 接收 ESP32 通过 UDP 发来的音频，存成 WAV 文件。
只用 Python 标准库，无需 pip 安装。

用法：
    python save_wav.py
    （对着 ESP32 麦克风说话）
    说完按 Ctrl+C 停止 → 生成 recording.wav → 用播放器试听

能听清 = 录音没问题（OpenAI 也能识别）
听不清/没声 = 录音有问题（增益/接线/时机）
"""
import socket
import wave
import array

UDP_PORT  = 8888
RATE      = 16000
OUT_FILE  = "recording.wav"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(("0.0.0.0", UDP_PORT))
print(f"监听 UDP {UDP_PORT}，等待 ESP32 数据...")
print("对着麦克风说话，看「峰值」是否随说话跳大。说完 Ctrl+C 保存。\n")

frames = bytearray()
total  = 0
acc    = bytearray()
try:
    while True:
        data, _ = sock.recvfrom(4096)
        frames += data
        acc    += data
        total  += len(data)
        # 每约 0.3 秒算一次峰值
        if len(acc) >= RATE:          # ~0.3s 的字节
            samples = array.array('h', acc[:len(acc) // 2 * 2])
            peak = max(abs(s) for s in samples) if samples else 0
            print(f"接收 {total//1024} KB   峰值: {peak}")
            acc = bytearray()
except KeyboardInterrupt:
    print("\n停止，写入文件...")

with wave.open(OUT_FILE, "wb") as w:
    w.setnchannels(1)        # 单声道
    w.setsampwidth(2)        # 16-bit
    w.setframerate(RATE)     # 16kHz
    w.writeframes(bytes(frames))

print(f"已保存 {OUT_FILE}（{len(frames)//1024} KB，{len(frames)//(RATE*2)} 秒）")
print("用播放器打开 recording.wav 试听。")

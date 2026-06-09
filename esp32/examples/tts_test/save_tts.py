"""
save_tts.py — TCP 接收 ESP32 发来的 TTS 音频，存成 WAV（可靠，不丢包）。
只用标准库。

用法：
    python save_tts.py          # 先启动，当 TCP 服务器
    （烧录 ESP32 tts_test，它会连过来发音频）
    → ESP32 发完关闭连接 → 自动生成 tts.wav → 播放试听
"""
import socket
import wave

PORT     = 8889
RATE     = 24000          # OpenAI TTS 输出 24kHz
OUT_FILE = "tts.wav"

srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("0.0.0.0", PORT))
srv.listen(1)
print(f"TCP 服务器监听 {PORT}，等待 ESP32 连接...")

conn, addr = srv.accept()
print(f"ESP32 已连接: {addr}")

frames = bytearray()
while True:
    data = conn.recv(4096)
    if not data:                 # 连接关闭 = 发送完毕
        break
    frames += data
    print(f"已接收 {len(frames)//1024} KB", end="\r")

conn.close()
print(f"\n接收完毕，写入文件...")

with wave.open(OUT_FILE, "wb") as w:
    w.setnchannels(1)        # 单声道
    w.setsampwidth(2)        # 16-bit
    w.setframerate(RATE)     # 24kHz
    w.writeframes(bytes(frames))

secs = len(frames) / (RATE * 2)
print(f"已保存 {OUT_FILE}（{len(frames)//1024} KB，{secs:.1f} 秒）")
print("用播放器打开 tts.wav 试听。")

# uart_link.py — K230 ↔ ESP32 UART 桥（K230 端）
#
# 协议：NDJSON，一行一条 JSON，以 '\n' 结尾。详见 shared/protocol/protocol.md。
# 引脚/UART 编号统一在 board.py（以庐山派原理图为准）。
#
# 用法：
#   from uart_link import UartLink
#   link = UartLink()
#   link.send({"t": "face", "x": 0.3, "n": 1})   # 发
#   for msg in link.poll():                        # 收（非阻塞，主循环里调）
#       print(msg)                                 # msg 是 dict

import ujson
from machine import UART, FPIOA
from board import UART_ID, UART_TX, UART_RX, UART_BAUD


class UartLink:
    def __init__(self):
        # K230 必须先用 FPIOA 把引脚复用到 UART 功能，再建 UART 对象
        fpioa = FPIOA()
        try:
            # 用实例属性取功能常量（与官方 demo 一致）
            fpioa.set_function(UART_TX, getattr(fpioa, "UART%d_TXD" % UART_ID))
            fpioa.set_function(UART_RX, getattr(fpioa, "UART%d_RXD" % UART_ID))
        except Exception as e:
            raise RuntimeError(
                "FPIOA 复用失败: GPIO%d/%d 可能不属于 UART%d。"
                "请改 board.py 的 UART_ID（试 1 或其它），错误: %s"
                % (UART_TX, UART_RX, UART_ID, e))

        # 只取需要的那一路常量（避免引用不存在的 UART0/控制台）
        uart_const = getattr(UART, "UART%d" % UART_ID)
        self._uart = UART(uart_const, baudrate=UART_BAUD,
                          bits=UART.EIGHTBITS, parity=UART.PARITY_NONE,
                          stop=UART.STOPBITS_ONE)
        self._buf = b""
        print("UART 桥就绪: UART%d TX=%d RX=%d @%d" %
              (UART_ID, UART_TX, UART_RX, UART_BAUD))

    def send(self, obj):
        """发一条消息。obj 是 dict，自动转 JSON 并补 '\n'。"""
        line = ujson.dumps(obj) + "\n"
        self._uart.write(line)

    def poll(self):
        """非阻塞读，返回这次读到的完整消息列表（每个是 dict）。
        在主循环里反复调用。解析失败的行会被丢弃。"""
        data = self._uart.read()        # 无数据返回 None
        msgs = []
        if not data:
            return msgs

        self._buf += data
        while b"\n" in self._buf:
            raw, self._buf = self._buf.split(b"\n", 1)
            raw = raw.strip()
            if not raw:
                continue
            try:
                msgs.append(ujson.loads(raw))
            except Exception:
                print("丢弃非法 JSON:", raw)
        return msgs

# probe_fpioa.py — 查 GPIO14/15 在 FPIOA 里到底能复用成哪些功能
#
# 跑这个，看输出里 GPIO14/15 各自支持的 UARTx_TXD / UARTx_RXD 是几号，
# 然后把 board.py 的 UART_ID 改成那个号。

from machine import FPIOA

fpioa = FPIOA()

print("==== GPIO14 可用功能 ====")
fpioa.help(14)

print("\n==== GPIO15 可用功能 ====")
fpioa.help(15)

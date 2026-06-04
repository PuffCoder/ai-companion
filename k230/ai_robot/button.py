from machine import Pin
import time
from config import BTN_GPIO

_btn = Pin(BTN_GPIO, Pin.IN, Pin.PULL_DOWN)

def is_pressed():
    return _btn.value() == 1

def wait_press():
    """等待一次完整按下+松开"""
    while not is_pressed():   # 等按下
        time.sleep_ms(10)
    time.sleep_ms(50)         # 消抖
    while is_pressed():       # 等松开
        time.sleep_ms(10)
    time.sleep_ms(50)
    print("按键触发")

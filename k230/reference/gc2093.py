from libs.PipeLine import PipeLine
import time

pl = PipeLine(
    rgb888p_size=[640, 480],
    display_mode="virt",
    display_size=[640, 480]
)
pl.create(sensor_id=2)   # CSI1

print("摄像头启动，Frame Buffer 应该有画面")

while True:
    img = pl.get_frame()
    pl.show_image()
    time.sleep_ms(30)

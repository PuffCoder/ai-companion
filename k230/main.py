# main.py — K230 开机自动运行入口（人脸检测 → UART 发 face_x 给 ESP32）
#
# 放 SD 卡根目录：/sdcard/main.py（不是 /sdcard/ai_robot/ 里面）。
# 依赖 /sdcard/ai_robot/ 下的 uart_link.py、board.py。
# CanMV 上电（未连 IDE）时自动执行本文件；连 IDE 会接管（可打断）。

# ⚠️ 路径设置必须放在所有 import * 之前，否则 sys 会被通配符导入覆盖
import os, sys, time
sys.path.append("/sdcard/ai_robot")

# 开机留缓冲：这几秒内连上 IDE 可打断，方便改代码（否则死循环抢不回控制权）
print("K230 启动中... 3 秒内连 IDE 可打断")
time.sleep(3)

from libs.PipeLine import PipeLine
from libs.AIBase import AIBase
from libs.AI2D import Ai2d
from libs.Utils import *
import ujson, gc, math
from media.media import *
import nncase_runtime as nn
import ulab.numpy as np
import image
import aidemo

from uart_link import UartLink


class FaceDetectionApp(AIBase):
    def __init__(self, kmodel_path, model_input_size, anchors, confidence_threshold=0.5,
                 nms_threshold=0.2, rgb888p_size=[1280, 720], display_size=[1920, 1080], debug_mode=0):
        super().__init__(kmodel_path, model_input_size, rgb888p_size, debug_mode)
        self.kmodel_path = kmodel_path
        self.model_input_size = model_input_size
        self.confidence_threshold = confidence_threshold
        self.nms_threshold = nms_threshold
        self.anchors = anchors
        self.rgb888p_size = [ALIGN_UP(rgb888p_size[0], 16), rgb888p_size[1]]
        self.display_size = [ALIGN_UP(display_size[0], 16), display_size[1]]
        self.debug_mode = debug_mode
        self.ai2d = Ai2d(debug_mode)
        self.ai2d.set_ai2d_dtype(nn.ai2d_format.NCHW_FMT, nn.ai2d_format.NCHW_FMT, np.uint8, np.uint8)

    def config_preprocess(self, input_image_size=None):
        with ScopedTiming("set preprocess config", self.debug_mode > 0):
            ai2d_input_size = input_image_size if input_image_size else self.rgb888p_size
            top, bottom, left, right, _ = letterbox_pad_param(self.rgb888p_size, self.model_input_size)
            self.ai2d.pad([0, 0, 0, 0, top, bottom, left, right], 0, [104, 117, 123])
            self.ai2d.resize(nn.interp_method.tf_bilinear, nn.interp_mode.half_pixel)
            self.ai2d.build([1, 3, ai2d_input_size[1], ai2d_input_size[0]],
                            [1, 3, self.model_input_size[1], self.model_input_size[0]])

    def postprocess(self, results):
        with ScopedTiming("postprocess", self.debug_mode > 0):
            post_ret = aidemo.face_det_post_process(self.confidence_threshold, self.nms_threshold,
                                                    self.model_input_size[1], self.anchors,
                                                    self.rgb888p_size, results)
            if len(post_ret) == 0:
                return post_ret
            else:
                return post_ret[0]


def run():
    display_mode = "virt"          # 无 IDE 也能跑；连 IDE 时可看画面
    display_size = None
    rgb888p_size = [1280, 720]

    kmodel_path  = "/sdcard/examples/kmodel/face_detection_320.kmodel"
    anchors_path = "/sdcard/examples/utils/prior_data_320.bin"
    anchors = np.fromfile(anchors_path, dtype=np.float).reshape((4200, 4))

    pl = PipeLine(rgb888p_size=rgb888p_size, display_mode=display_mode, display_size=display_size)
    pl.create(sensor_id=1)          # 摄像头在 CSI1
    pl.sensor.set_vflip(True)       # 摄像头上下颠倒 → 翻正（装反了改这里）
    display_size = pl.get_display_size()
    disp_w, disp_h = display_size[0], display_size[1]

    face_det = FaceDetectionApp(kmodel_path, model_input_size=[320, 320], anchors=anchors,
                                confidence_threshold=0.5, nms_threshold=0.2,
                                rgb888p_size=rgb888p_size, display_size=display_size, debug_mode=0)
    face_det.config_preprocess()

    link = UartLink()
    print("人脸检测就绪，UART 已连")

    last_send = time.ticks_ms()
    SEND_MS = 33                    # UART ~30Hz，跟随丝滑

    while True:
        with ScopedTiming("total", 0):
            img = pl.get_frame()
            dets = face_det.run(img)
            pl.osd_img.clear()

            now = time.ticks_ms()
            do_send = time.ticks_diff(now, last_send) >= SEND_MS
            if do_send:
                last_send = now

            pl.osd_img.draw_line(disp_w // 2, 0, disp_w // 2, disp_h, color=(255, 128, 128, 128), thickness=2)

            if dets:
                biggest = max(dets, key=lambda d: d[2] * d[3])
                x, y, w, h = map(lambda v: int(round(v, 0)), biggest[:4])
                x = x * disp_w // face_det.rgb888p_size[0]
                y = y * disp_h // face_det.rgb888p_size[1]
                w = w * disp_w // face_det.rgb888p_size[0]
                h = h * disp_h // face_det.rgb888p_size[1]
                cx = x + w // 2
                face_x = cx / disp_w * 2.0 - 1.0

                pl.osd_img.draw_rectangle(x, y, w, h, color=(255, 255, 0, 255), thickness=4)
                pl.osd_img.draw_string_advanced(x, max(y - 40, 0), 32, "x=%.2f" % face_x,
                                                color=(255, 255, 0, 0))
                if do_send:
                    link.send({"t": "face", "x": round(face_x, 2), "n": len(dets)})
            else:
                if do_send:
                    link.send({"t": "noface"})

            pl.show_image()
            gc.collect()


# 出错不让开机卡死，打印异常方便排查
try:
    run()
except Exception as e:
    print("main.py error:", e)

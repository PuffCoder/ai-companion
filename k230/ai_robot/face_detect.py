# face_detect.py — K230 人脸检测 → 算 face_x → UART 发给 ESP32
#
# 基于官方 face_detection 例程（FaceDetectionApp），只加了 3 件事：
#   1) 算 face_x（最大人脸中心归一化 -1..1）
#   2) UartLink.send({"t":"face","x":..,"n":..}) / {"t":"noface"}
#   3) 画屏中线 + face_x 数值，方便在 IDE 里调试
#
# 在 CanMV IDE 里运行：IDE 显示画面（人脸框+中线+x值），同时驱动 ESP32 舵机。

# ⚠️ 路径设置必须放在所有 import * 之前，否则 sys 会被通配符导入覆盖
import os, sys
sys.path.append("/sdcard/ai_robot")

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


if __name__ == "__main__":
    display_mode = "virt"          # IDE 里看用 virt；接 HDMI 屏改 "hdmi"
    display_size = None            # 用默认分辨率（和能跑通的 yolov8 例程一致）
    rgb888p_size = [1280, 720]

    kmodel_path  = "/sdcard/examples/kmodel/face_detection_320.kmodel"
    anchors_path = "/sdcard/examples/utils/prior_data_320.bin"
    confidence_threshold = 0.5
    nms_threshold = 0.2
    anchor_len = 4200
    det_dim = 4
    anchors = np.fromfile(anchors_path, dtype=np.float).reshape((anchor_len, det_dim))

    pl = PipeLine(rgb888p_size=rgb888p_size, display_mode=display_mode, display_size=display_size)
    pl.create(sensor_id=1)          # 你的摄像头在 CSI1（和 gc2093/yolov8 例程一致）
    display_size = pl.get_display_size()
    disp_w, disp_h = display_size[0], display_size[1]

    face_det = FaceDetectionApp(kmodel_path, model_input_size=[320, 320], anchors=anchors,
                                confidence_threshold=confidence_threshold, nms_threshold=nms_threshold,
                                rgb888p_size=rgb888p_size, display_size=display_size, debug_mode=0)
    face_det.config_preprocess()

    link = UartLink()
    print("人脸检测就绪，UART 已连")

    while True:
        with ScopedTiming("total", 0):
            img = pl.get_frame()
            dets = face_det.run(img)          # rgb888p 坐标下的人脸框列表 [x,y,w,h,...]
            pl.osd_img.clear()

            # 屏中线（看脸偏左还是偏右）
            pl.osd_img.draw_line(disp_w // 2, 0, disp_w // 2, disp_h, color=(255, 128, 128, 128), thickness=2)

            if dets:
                # 最大的脸（离得最近/最主要那张）
                biggest = max(dets, key=lambda d: d[2] * d[3])
                x, y, w, h = map(lambda v: int(round(v, 0)), biggest[:4])
                # rgb888p 坐标 → 显示坐标
                x = x * disp_w // face_det.rgb888p_size[0]
                y = y * disp_h // face_det.rgb888p_size[1]
                w = w * disp_w // face_det.rgb888p_size[0]
                h = h * disp_h // face_det.rgb888p_size[1]
                cx = x + w // 2
                face_x = cx / disp_w * 2.0 - 1.0     # 归一化 -1(最左)..+1(最右)

                pl.osd_img.draw_rectangle(x, y, w, h, color=(255, 255, 0, 255), thickness=4)
                pl.osd_img.draw_string_advanced(x, max(y - 40, 0), 32, "x=%.2f" % face_x,
                                                color=(255, 255, 0, 0))

                link.send({"t": "face", "x": round(face_x, 2), "n": len(dets)})
            else:
                link.send({"t": "noface"})

            pl.show_image()
            gc.collect()

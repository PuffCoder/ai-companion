#pragma once

#include <stdint.h>

/* INMP441 I2S 麦克风驱动。引脚见 bsp_board.h。
 * INMP441 是 24 位麦克风：内部用 32 位 I2S 槽读取，再缩放为 16 位 PCM。 */

#define MIC_SAMPLE_RATE   16000   /* 采样率 */

/**
 * @brief 初始化 I2S 麦克风。
 */
void mic_init(void);

/**
 * @brief 读取音频，输出 16 位 PCM（已缩放）。
 * @param out         输出缓冲（int16）
 * @param max_samples out 的容量（采样数）
 * @return 实际读到的采样数
 */
int mic_read(int16_t *out, int max_samples);

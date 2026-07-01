#pragma once

/*
 * emotion_engine — 在 ST7789 上画像素风"眼睛"表情。
 * 一张低分辨率点阵：每个"大像素" = 一块 fill_rect。
 * 表情在 .c 里用 5x5 数组定义，好看好改。
 *
 * 用法：
 *   emotion_engine_init();
 *   emotion_set(EMO_HAPPY);   // 切表情
 *   emotion_blink();          // 眨一下（当前表情）
 */

typedef enum {
    EMO_IDLE = 0,     /* 待机：方块像素眼（复古机器人风，保留原方案）*/
    EMO_LISTENING,    /* 在听：圆角睁大、专注 */
    EMO_THINKING,     /* 思考：眼睛看上方 */
    EMO_HAPPY,        /* 开心：弯眼笑 ⌒ */
    EMO_SAD,          /* 难过：圆角眼 + 八字眉 */
    EMO_SURPRISED,    /* 惊讶：瞪圆 + 抬眉 */
    EMO_ANGRY,        /* 生气：圆角眼 + 倒八字眉 */
    EMO_COUNT
} emotion_t;

/* IDLE 用方块像素，其余用圆角眼 + 眉毛 + 平滑眨眼 */

/** 初始化屏 + 背光 + 清屏，默认 IDLE。 */
void emotion_engine_init(void);

/** 切换到某个表情并绘制。 */
void emotion_set(emotion_t e);

/** 眨一下眼：闭眼→短暂停→恢复当前表情。 */
void emotion_blink(void);

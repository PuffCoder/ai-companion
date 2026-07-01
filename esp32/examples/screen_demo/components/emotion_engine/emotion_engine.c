#include "emotion_engine.h"
#include "display_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <stdlib.h>

static const char *TAG = "emotion";

/* ── 布局：横屏 320x170，两眼中心 ─────────────────────── */
#define CX_L       90
#define CX_R      230
#define CY         85
#define BROW_TH     7      /* 眉毛粗细 */

/* 每只眼一个离屏缓冲（含眉毛区）：宽 84（盖住 80px 方块眼），y 24..128 */
#define CLR_HW     42
#define FB_W      (CLR_HW * 2)   /* 84 */
#define FB_H      104
#define FB_Y       24

/* 字节交换后的颜色（blit 直接用）*/
#define SW(c)     ((uint16_t)(((c) >> 8) | ((c) << 8)))
#define EYE_SW     SW(COLOR_CYAN)
#define BG_SW      SW(COLOR_BLACK)

static emotion_t s_emo = EMO_IDLE;
static uint16_t *s_fb;            /* FB_W*FB_H 缓冲（DMA 可用）*/
static int s_bx, s_by;           /* 当前眼缓冲在屏上的原点（屏坐标→缓冲）*/

/* ── 缓冲绘制原语（屏坐标输入，写进缓冲）───────────────── */
static void fb_clear(void)
{
    for (int i = 0; i < FB_W * FB_H; i++) s_fb[i] = BG_SW;
}
static void put_hspan(int sx, int sy, int w, uint16_t c)
{
    int ly = sy - s_by;
    if (ly < 0 || ly >= FB_H) return;
    int lx = sx - s_bx;
    for (int i = 0; i < w; i++) {
        int x = lx + i;
        if (x >= 0 && x < FB_W) s_fb[ly * FB_W + x] = c;
    }
}
static void put_rect(int sx, int sy, int w, int h, uint16_t c)
{
    for (int r = 0; r < h; r++) put_hspan(sx, sy + r, w, c);
}

/* ── IDLE：方块像素眼 ───────────────────────────────── */
#define B_BLOCK    16
static const uint8_t IDLE_PAT[5][5] = {
    {0,1,1,1,0},{1,1,1,1,1},{1,1,1,1,1},{1,1,1,1,1},{0,1,1,1,0},
};
static void draw_blocky_eye(int cx)
{
    int ex = cx - 40, ey = CY - 40;
    for (int r = 0; r < 5; r++)
        for (int c = 0; c < 5; c++)
            if (IDLE_PAT[r][c])
                put_rect(ex + c * B_BLOCK, ey + r * B_BLOCK, B_BLOCK, B_BLOCK, EYE_SW);
}

/* ── 圆角眼 + 眉毛 ──────────────────────────────────── */
static void fill_round(int cx, int cy, int w, int h, int r)
{
    if (h < 2) h = 2;
    if (r > h / 2) r = h / 2;
    if (r > w / 2) r = w / 2;
    int x0 = cx - w / 2, y0 = cy - h / 2;
    for (int dy = 0; dy < h; dy++) {
        int td = dy, bd = h - 1 - dy, inset = 0;
        if (td < r) { int v = r - td; inset = r - (int)lroundf(sqrtf((float)(r*r - v*v))); }
        if (bd < r) { int v = r - bd; int in2 = r - (int)lroundf(sqrtf((float)(r*r - v*v)));
                      if (in2 > inset) inset = in2; }
        put_hspan(x0 + inset, y0 + dy, w - 2 * inset, EYE_SW);
    }
}

static void draw_line(int x1, int y1, int x2, int y2, int th)
{
    int steps = abs(x2 - x1);
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; i++) {
        int x = x1 + (x2 - x1) * i / steps;
        int y = y1 + (y2 - y1) * i / steps;
        put_rect(x - 1, y - th / 2, 2, th, EYE_SW);
    }
}

static void draw_brow_eye(emotion_t e, int cx, int right)
{
    if (e == EMO_ANGRY) {           /* 倒八字：内低外高 */
        if (!right) draw_line(cx - 22, 36, cx + 14, 48, BROW_TH);
        else        draw_line(cx - 14, 48, cx + 22, 36, BROW_TH);
    } else if (e == EMO_SAD) {      /* 八字：内高外低 */
        if (!right) draw_line(cx - 22, 48, cx + 14, 36, BROW_TH);
        else        draw_line(cx - 14, 36, cx + 22, 48, BROW_TH);
    } else if (e == EMO_SURPRISED) {/* 抬眉横线 */
        draw_line(cx - 20, 34, cx + 20, 34, BROW_TH);
    }
}

static void draw_round_eye(int cx, emotion_t e, int hpct)
{
    switch (e) {
        case EMO_LISTENING: fill_round(cx, CY,      52, 74 * hpct / 100, 22); break;
        case EMO_THINKING:  fill_round(cx, CY - 14, 48, 60 * hpct / 100, 22); break;
        case EMO_SAD:       fill_round(cx, CY + 4,  46, 56 * hpct / 100, 20); break;
        case EMO_SURPRISED: fill_round(cx, CY,      60, 74 * hpct / 100, 30); break;
        case EMO_ANGRY:     fill_round(cx, CY + 2,  50, 60 * hpct / 100, 18); break;
        case EMO_HAPPY:     fill_round(cx, CY + 8,  54, 24 * hpct / 100, 12); break;
        default:            fill_round(cx, CY,      50, 68 * hpct / 100, 20); break;
    }
}

/* 渲染整张脸：每只眼在缓冲里画好，再一次 blit（无频闪）*/
static void render(emotion_t e, int hpct)
{
    for (int side = 0; side < 2; side++) {
        int cx = side ? CX_R : CX_L;
        s_bx = cx - CLR_HW;
        s_by = FB_Y;
        fb_clear();

        if (e == EMO_IDLE) {
            draw_blocky_eye(cx);                 /* 先画满 */
            int vh = 40 * hpct / 100;            /* 可见半高，随 hpct 平滑收缩 */
            put_rect(cx - 40, CY - 40, 80, 40 - vh, BG_SW);   /* 上眼皮 */
            put_rect(cx - 40, CY + vh, 80, 40 - vh, BG_SW);   /* 下眼皮 */
        } else {
            draw_round_eye(cx, e, hpct);
            if (hpct > 60) draw_brow_eye(e, cx, side);
        }

        display_blit(s_bx, s_by, FB_W, FB_H, s_fb);
    }
}

/* ── Public API ─────────────────────────────────────── */
void emotion_engine_init(void)
{
    display_driver_init();
    display_set_backlight(true);
    display_fill(COLOR_BLACK);

    s_fb = heap_caps_malloc(FB_W * FB_H * sizeof(uint16_t), MALLOC_CAP_DMA);
    if (!s_fb) { ESP_LOGE(TAG, "framebuffer alloc failed"); return; }

    emotion_set(EMO_IDLE);
    ESP_LOGI(TAG, "emotion engine ready");
}

void emotion_set(emotion_t e)
{
    if (e >= EMO_COUNT) e = EMO_IDLE;
    s_emo = e;
    render(e, 100);
}

void emotion_blink(void)
{
    static const int seq[] = { 90, 65, 40, 15, 40, 65, 90, 100 };
    for (int i = 0; i < 8; i++) {
        render(s_emo, seq[i]);
        vTaskDelay(pdMS_TO_TICKS(55));
    }
}

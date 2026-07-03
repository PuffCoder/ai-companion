#include "display_driver.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "display_driver";

static spi_device_handle_t s_spi;

/* ── ST7789 Commands ─────────────────────────────────────────── */
#define ST7789_SWRESET  0x01
#define ST7789_SLPOUT   0x11
#define ST7789_NORON    0x13
#define ST7789_INVON    0x21
#define ST7789_DISPON   0x29
#define ST7789_CASET    0x2A
#define ST7789_RASET    0x2B
#define ST7789_RAMWR    0x2C
#define ST7789_MADCTL   0x36
#define ST7789_COLMOD   0x3A

/* ── DC pin via SPI pre/post transaction callbacks ───────────── */
static void IRAM_ATTR lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    int dc = (int)t->user;
    gpio_set_level(DISPLAY_PIN_DC, dc);
}

/* ── Low-level SPI helpers ───────────────────────────────────── */

static void lcd_cmd(uint8_t cmd)
{
    spi_transaction_t t = {
        .length   = 8,
        .tx_buffer = &cmd,
        .user     = (void *)0,  // DC=0: command
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void lcd_data(const uint8_t *data, size_t len)
{
    if (len == 0) return;
    spi_transaction_t t = {
        .length    = len * 8,
        .tx_buffer = data,
        .user      = (void *)1,  // DC=1: data
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void lcd_data_byte(uint8_t data)
{
    lcd_data(&data, 1);
}

/* ── Set draw window ─────────────────────────────────────────── */

static void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    y0 += 35; y1 += 35;  // 横屏：170 那一维在 Y/行，偏移 35（竖屏时是 x0/x1 +=35）

    uint8_t caset[] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t raset[] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };

    lcd_cmd(ST7789_CASET);
    lcd_data(caset, 4);
    lcd_cmd(ST7789_RASET);
    lcd_data(raset, 4);
    lcd_cmd(ST7789_RAMWR);
}

/* ── Init sequence ───────────────────────────────────────────── */

static void st7789_init_sequence(void)
{
    ESP_LOGI(TAG, "Hardware reset...");
    gpio_set_level(DISPLAY_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(DISPLAY_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_LOGI(TAG, "Sending init commands...");

    lcd_cmd(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    lcd_cmd(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Memory data access control（方向）
    //  竖屏 170x320: 0x00 / 0xC0（翻转）
    //  横屏 320x170: 0x60 / 0xA0（翻转）  ← 当前用横屏
    //  如果镜像/上下颠倒，就在 0x60/0xA0/0xC0/0xE0 之间换
    lcd_cmd(ST7789_MADCTL);
    lcd_data_byte(0xA0);

    // Color format: 16-bit RGB565
    lcd_cmd(ST7789_COLMOD);
    lcd_data_byte(0x55);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Porch setting
    lcd_cmd(0xB2);
    uint8_t porch[] = {0x0C, 0x0C, 0x00, 0x33, 0x33};
    lcd_data(porch, 5);

    // Gate control
    lcd_cmd(0xB7);
    lcd_data_byte(0x35);

    // VCOM setting
    lcd_cmd(0xBB);
    lcd_data_byte(0x19);

    // LCM control
    lcd_cmd(0xC0);
    lcd_data_byte(0x2C);

    // VDV and VRH command enable
    lcd_cmd(0xC2);
    lcd_data_byte(0x01);

    // VRH set
    lcd_cmd(0xC3);
    lcd_data_byte(0x12);

    // VDV set
    lcd_cmd(0xC4);
    lcd_data_byte(0x20);

    // Frame rate (60Hz)
    lcd_cmd(0xC6);
    lcd_data_byte(0x0F);

    // Power control 1
    lcd_cmd(0xD0);
    uint8_t pwr[] = {0xA4, 0xA1};
    lcd_data(pwr, 2);

    // Positive gamma
    lcd_cmd(0xE0);
    uint8_t pgamma[] = {0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,0x4C,0x18,0x0D,0x0B,0x1F,0x23};
    lcd_data(pgamma, 14);

    // Negative gamma
    lcd_cmd(0xE1);
    uint8_t ngamma[] = {0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,0x51,0x2F,0x1F,0x1F,0x20,0x23};
    lcd_data(ngamma, 14);

    // IPS inversion ON (required for IPS panels)
    lcd_cmd(ST7789_INVON);
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_cmd(ST7789_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));

    lcd_cmd(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(120));

    ESP_LOGI(TAG, "ST7789 init done");
}

/* ── Public API ──────────────────────────────────────────────── */

void display_driver_init(void)
{
    ESP_LOGI(TAG, "Initializing GPIO...");

    // Backlight
    gpio_config_t bl_cfg = {
        .pin_bit_mask = (1ULL << DISPLAY_PIN_BL),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&bl_cfg));
    gpio_set_level(DISPLAY_PIN_BL, 1);

    // RST & DC
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << DISPLAY_PIN_RST) | (1ULL << DISPLAY_PIN_DC),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_cfg));
    gpio_set_level(DISPLAY_PIN_DC, 1);
    gpio_set_level(DISPLAY_PIN_RST, 1);

    ESP_LOGI(TAG, "Initializing SPI bus...");

    // SPI bus
    spi_bus_config_t bus_cfg = {
        .mosi_io_num     = DISPLAY_PIN_MOSI,
        .miso_io_num     = -1,
        .sclk_io_num     = DISPLAY_PIN_SCLK,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * 2 + 8,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // SPI device — use pre_cb to control DC pin
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz  = DISPLAY_SPI_FREQ_HZ,
        .mode            = 0,
        .spics_io_num    = DISPLAY_PIN_CS,
        .queue_size      = 7,
        .pre_cb          = lcd_spi_pre_transfer_callback,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(SPI2_HOST, &dev_cfg, &s_spi));

    ESP_LOGI(TAG, "SPI ready, running ST7789 init...");
    st7789_init_sequence();

    ESP_LOGI(TAG, "Filling screen black...");
    display_fill(COLOR_BLACK);

    ESP_LOGI(TAG, "Display initialized (%dx%d)", DISPLAY_WIDTH, DISPLAY_HEIGHT);
}

void display_fill(uint16_t color)
{
    display_fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}

void display_fill_rect(uint16_t x, uint16_t y,
                       uint16_t w, uint16_t h,
                       uint16_t color)
{
    set_window(x, y, x + w - 1, y + h - 1);

    // Swap bytes for big-endian SPI
    uint16_t swapped = (color >> 8) | (color << 8);

    static uint16_t buf[128];
    for (int i = 0; i < 128; i++) buf[i] = swapped;

    uint32_t total = (uint32_t)w * h;
    while (total > 0) {
        uint32_t chunk = total > 128 ? 128 : total;
        spi_transaction_t t = {
            .length    = chunk * 16,
            .tx_buffer = buf,
            .user      = (void *)1,  // DC=1: data
        };
        ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
        total -= chunk;
    }
}

void display_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    display_fill_rect(x, y, 1, 1, color);
}

void display_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels)
{
    set_window(x, y, x + w - 1, y + h - 1);
    spi_transaction_t t = {
        .length    = (size_t)w * h * 16,
        .tx_buffer = pixels,
        .user      = (void *)1,   // DC=1: data
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

void display_set_backlight(bool on)
{
    gpio_set_level(DISPLAY_PIN_BL, on ? 1 : 0);
}

#include "as5600.h"
#include "bsp_board.h"

#include "driver/i2c_master.h"
#include "esp_log.h"

static const char *TAG = "as5600";

#define AS5600_ADDR        0x36
#define REG_STATUS         0x0B
#define REG_RAW_ANGLE_H    0x0C   /* 0x0C MSB, 0x0D LSB */
#define REG_AGC            0x1A
#define REG_MAGNITUDE_H    0x1B   /* 0x1B MSB, 0x1C LSB */

static i2c_master_bus_handle_t s_bus;
static i2c_master_dev_handle_t s_dev;

/* 写寄存器地址，再读 len 字节 */
static esp_err_t read_regs(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, 100 /*ms*/);
}

esp_err_t as5600_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .clk_source        = I2C_CLK_SRC_DEFAULT,
        .i2c_port          = BSP_AS5600_I2C_PORT,
        .sda_io_num        = BSP_AS5600_I2C_SDA,
        .scl_io_num        = BSP_AS5600_I2C_SCL,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_cfg, &s_bus));

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = AS5600_ADDR,
        .scl_speed_hz    = BSP_AS5600_I2C_HZ,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev));

    /* 探测：读一次 STATUS */
    esp_err_t err = i2c_master_probe(s_bus, AS5600_ADDR, 100);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "AS5600 未应答 (addr 0x%02X) — 检查接线/上电", AS5600_ADDR);
        return ESP_ERR_NOT_FOUND;
    }

    ESP_LOGI(TAG, "AS5600 ready (SDA=%d SCL=%d)", BSP_AS5600_I2C_SDA, BSP_AS5600_I2C_SCL);
    return ESP_OK;
}

esp_err_t as5600_read_raw(uint16_t *out_raw)
{
    uint8_t b[2];
    esp_err_t err = read_regs(REG_RAW_ANGLE_H, b, 2);
    if (err != ESP_OK) return err;
    *out_raw = (((uint16_t)b[0] << 8) | b[1]) & 0x0FFF;   /* 12-bit */
    return ESP_OK;
}

esp_err_t as5600_read_deg(float *out_deg)
{
    uint16_t raw;
    esp_err_t err = as5600_read_raw(&raw);
    if (err != ESP_OK) return err;
    *out_deg = raw * (360.0f / 4096.0f);
    return ESP_OK;
}

esp_err_t as5600_read_status(uint8_t *out_status)
{
    return read_regs(REG_STATUS, out_status, 1);
}

esp_err_t as5600_read_agc(uint8_t *out_agc)
{
    return read_regs(REG_AGC, out_agc, 1);
}

esp_err_t as5600_read_magnitude(uint16_t *out_mag)
{
    uint8_t b[2];
    esp_err_t err = read_regs(REG_MAGNITUDE_H, b, 2);
    if (err != ESP_OK) return err;
    *out_mag = (((uint16_t)b[0] << 8) | b[1]) & 0x0FFF;
    return ESP_OK;
}

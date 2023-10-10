

/* parts from
    https://github.com/electricimp/LIS3DH/blob/master/LIS3DH.device.lib.nut
*/

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "lis3dh.h"
#include "lis3dh_regs.h"
#include "i2c_bus.h"

static const char *TAG = "LIS3DH";

esp_err_t lis3dh_get_reg(lis3dh_t ctx, uint8_t reg, uint8_t *val)
{
    return i2c_bus_read_bytes(ctx->i2c_handle, LIS3DH_ADDR, &reg, 1, val, 1);
}

esp_err_t lis3dh_set_reg(lis3dh_t ctx, uint8_t reg, uint8_t val)
{
    return i2c_bus_write_bytes(ctx->i2c_handle, LIS3DH_ADDR, &reg, 1, &val, 1);
}

esp_err_t lis3dh_fetch(lis3dh_t ctx, float *measurements)
{
    uint8_t reading[6];
    esp_err_t ret = 0;

    for (int pos = 0; pos < sizeof(reading); pos++)
    {
        ret |= lis3dh_get_reg(ctx, LIS3DH_OUT_X_L_INCR + pos, &reading[pos]);
    }

    // Read and sign extend
    int16_t val_x = (reading[0] | (reading[1] << 8));
    int16_t val_y = (reading[2] | (reading[3] << 8));
    int16_t val_z = (reading[4] | (reading[5] << 8));

    // multiply by full-scale range to return in G
    measurements[0] = (val_x / 32000.0) * ctx->range;
    measurements[1] = (val_y / 32000.0) * ctx->range;
    measurements[2] = (val_z / 32000.0) * ctx->range;

    return (ret != 0) ? ESP_FAIL : ESP_OK;
}

esp_err_t lis3dh_get_range(lis3dh_t ctx)
{
    uint8_t range_bits = 0;
    lis3dh_get_reg(ctx, LIS3DH_CTRL_REG4, &range_bits);
    range_bits &= 0x30;
    range_bits >>= 4;

    if (range_bits == 0x00)
    {
        ctx->range = 2;
    }
    else if (range_bits == 0x01)
    {
        ctx->range = 4;
    }
    else if (range_bits == 0x02)
    {
        ctx->range = 8;
    }
    else
    {
        ctx->range = 16;
    }
    return ESP_OK;
}

// Set Accelerometer Data Rate in Hz
esp_err_t lis3dh_set_data_rate(lis3dh_t ctx, int rate)
{
    uint8_t val = 0;
    lis3dh_get_reg(ctx, LIS3DH_CTRL_REG1, &val);
    val &= 0x0F;
    bool normalMode = (val < 8);
    if (rate == 0)
    {
        rate = 0;
    }
    else if (rate <= 1)
    {
        val = val | 0x10;
        rate = 1;
    }
    else if (rate <= 10)
    {
        val = val | 0x20;
        rate = 10;
    }
    else if (rate <= 25)
    {
        val = val | 0x30;
        rate = 25;
    }
    else if (rate <= 50)
    {
        val = val | 0x40;
        rate = 50;
    }
    else if (rate <= 100)
    {
        val = val | 0x50;
        rate = 100;
    }
    else if (rate <= 200)
    {
        val = val | 0x60;
        rate = 200;
    }
    else if (rate <= 400)
    {
        val = val | 0x70;
        rate = 400;
    }
    else if (normalMode)
    {
        val = val | 0x90;
        rate = 1250;
    }
    else if (rate <= 1600)
    {
        val = val | 0x80;
        rate = 1600;
    }
    else
    {
        val = val | 0x90;
        rate = 5000;
    }
    lis3dh_set_reg(ctx, LIS3DH_CTRL_REG1, val);
    return rate;
}

lis3dh_t lis3dh_init(i2c_bus_handle_t i2c_handle)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Initialize");

    lis3dh_t ctx = calloc(1, sizeof(struct lis3dh_s));
    ctx->i2c_handle = i2c_handle;
    ctx->fetch = &lis3dh_fetch;
    ctx->set_data_rate = &lis3dh_set_data_rate;

    lis3dh_set_reg(ctx, LIS3DH_CTRL_REG1, 0x07);
    lis3dh_set_reg(ctx, LIS3DH_CTRL_REG2, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_CTRL_REG3, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_CTRL_REG4, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_CTRL_REG5, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_CTRL_REG6, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_INT1_CFG, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_INT1_THS, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_INT1_DURATION, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_CLICK_CFG, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_CLICK_THS, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_TIME_LIMIT, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_TIME_LATENCY, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_TIME_WINDOW, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_FIFO_CTRL_REG, 0x00);
    lis3dh_set_reg(ctx, LIS3DH_TEMP_CFG_REG, 0x00);

    lis3dh_get_range(ctx);

    return ctx;
}

esp_err_t lis3dh_deinit(lis3dh_t ctx)
{
    return ESP_OK;
}

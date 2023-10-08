/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2020 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include <string.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "i2c_bus.h"
#include "driver/gpio.h"
#include "board.h"

#include "dac3100.h"

static const char *TAG = "DAC3100";

static bool codec_init_flag;
static i2c_bus_handle_t i2c_handle;

audio_hal_func_t AUDIO_CODEC_DAC3100_DEFAULT_HANDLE = {
    .audio_codec_initialize = dac3100_init,
    .audio_codec_deinitialize = dac3100_deinit,
    .audio_codec_ctrl = dac3100_ctrl_state,
    .audio_codec_config_iface = dac3100_config_i2s,
    .audio_codec_set_mute = dac3100_set_mute,
    .audio_codec_set_volume = dac3100_set_volume,
    .audio_codec_get_volume = dac3100_get_volume,
};

static int i2c_init()
{
    int res;
    i2c_config_t dac3100_i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000};
    res = get_i2c_pins(I2C_NUM_0, &dac3100_i2c_cfg);
    i2c_handle = i2c_bus_create(I2C_NUM_0, &dac3100_i2c_cfg);

    if (!i2c_handle)
    {
        ESP_LOGE(TAG, "I2C init failed");
    }
    return res;
}

static esp_err_t dac3100_write_reg(uint8_t reg_add, uint8_t data)
{
    return i2c_bus_write_bytes(i2c_handle, DAC3100_ADDR, &reg_add, sizeof(reg_add), &data, sizeof(data));
}

static esp_err_t dac3100_read_reg(uint8_t reg_add, uint8_t *p_data)
{
    return i2c_bus_read_bytes(i2c_handle, DAC3100_ADDR, &reg_add, sizeof(reg_add), p_data, 1);
}

bool dac3100_initialized()
{
    return codec_init_flag;
}

void dac3100_read_all()
{
    for (int i = 0; i < 50; i++)
    {
        uint8_t reg = 0;
        dac3100_read_reg(i, &reg);
        ESP_LOGW(TAG, "  %x: %x", i, reg);
    }
}

esp_err_t dac3100_init(audio_hal_codec_config_t *cfg)
{
    ESP_LOGI(TAG, "dac3100 init");

    gpio_config_t io_conf = {0};
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = BIT64(DAC3100_RESET_GPIO);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    gpio_set_level(DAC3100_RESET_GPIO, 0);
    vTaskDelay(10 / portTICK_RATE_MS);
    gpio_set_level(DAC3100_RESET_GPIO, 1);
    vTaskDelay(10 / portTICK_RATE_MS);

    i2c_init();

    /* from datasheet */
    dac3100_write_reg(0x00, 0x00);
    dac3100_write_reg(0x01, 0x01);
    dac3100_write_reg(0x04, 0x03);
    dac3100_write_reg(0x06, 0x08);
    dac3100_write_reg(0x07, 0x00);
    dac3100_write_reg(0x08, 0x00);
    dac3100_write_reg(0x05, 0x91);
    dac3100_write_reg(0x0B, 0x88);
    dac3100_write_reg(0x0C, 0x82);
    dac3100_write_reg(0x0D, 0x00);
    dac3100_write_reg(0x0E, 0x80);
    dac3100_write_reg(0x1B, 0x00);
    dac3100_write_reg(0x3C, 0x0B);
    dac3100_write_reg(0x00, 0x08);
    dac3100_write_reg(0x01, 0x04);
    dac3100_write_reg(0x00, 0x00);
    dac3100_write_reg(0x74, 0x00);
    dac3100_write_reg(0x00, 0x01);
    dac3100_write_reg(0x1F, 0x04);
    dac3100_write_reg(0x21, 0x4E);
    dac3100_write_reg(0x23, 0x44);
    dac3100_write_reg(0x28, 0x06);
    dac3100_write_reg(0x29, 0x06);
    dac3100_write_reg(0x2A, 0x1C);
    dac3100_write_reg(0x1F, 0xC2);
    dac3100_write_reg(0x20, 0x86);
    dac3100_write_reg(0x24, 0x92);
    dac3100_write_reg(0x25, 0x92);
    dac3100_write_reg(0x26, 0x92);
    dac3100_write_reg(0x00, 0x00);
    dac3100_write_reg(0x3F, 0xD4);
    dac3100_write_reg(0x40, 0x00);


    return ESP_OK;
}

esp_err_t dac3100_deinit(void)
{
    return ESP_OK;
}

esp_err_t dac3100_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state)
{
    return ESP_OK;
}

esp_err_t dac3100_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface)
{
    return ESP_OK;
}

esp_err_t dac3100_set_mute(bool mute)
{
    return ESP_OK;
}

esp_err_t dac3100_set_volume(int volume)
{
    int value = -635 + (volume*635 / 100);
    uint8_t reg_val = value / 5;

    ESP_LOGW(TAG, "Volume: %d -> 0x%02X", volume, reg_val);
    dac3100_write_reg(0x00, 0x00);
    dac3100_write_reg(0x41, reg_val);
    dac3100_write_reg(0x42, reg_val);
    return ESP_OK;
}

esp_err_t dac3100_get_volume(int *volume)
{
    return ESP_OK;
}

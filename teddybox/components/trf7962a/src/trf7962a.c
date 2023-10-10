

/* parts from
    https://github.com/electricimp/trf7962a/blob/master/trf7962a.device.lib.nut
*/

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "trf7962a.h"
#include "trf7962a_regs.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

static const char *TAG = "TRF7962A";

esp_err_t trf7962a_get_reg(trf7962a_t ctx, uint8_t reg, uint8_t *val)
{
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));

    gpio_set_level(ctx->ss_gpio, 0);

    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 1;

    t.tx_data[0] = (reg & 0b00011111) | (uint8_t)REGISTER_B7 | (uint8_t)READ_B6;
    ret = spi_device_polling_transmit(ctx->spi_handle_write, &t);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed: %d", ret);
    }

    t.tx_data[0] = 0;
    t.rx_data[0] = 0;
    ret = spi_device_polling_transmit(ctx->spi_handle_read, &t);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed: %d", ret);
    }

    gpio_set_level(ctx->ss_gpio, 1);

    *val = t.rx_data[0];

    return ret;
}

esp_err_t trf7962a_set_reg(trf7962a_t ctx, uint8_t reg, uint8_t val)
{
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));
    t.flags |= SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 2;
    t.tx_data[0] = (reg & 0b00011111) | (uint8_t)REGISTER_B7 | (uint8_t)WRITE_B6;
    t.tx_data[1] = val;

    gpio_set_level(ctx->ss_gpio, 0);
    esp_err_t ret = spi_device_polling_transmit(ctx->spi_handle_write, &t);
    gpio_set_level(ctx->ss_gpio, 1);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed: %d", ret);
    }

    return ret;
}

esp_err_t trf7962a_command(trf7962a_t ctx, uint8_t cmd)
{
    spi_transaction_t t;

    memset(&t, 0, sizeof(t));
    t.flags |= SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 2;
    t.tx_data[0] = (cmd & 0b00011111) | (uint8_t)COMMAND_B7 | (uint8_t)WRITE_B6;
    t.tx_data[1] = 0;

    gpio_set_level(ctx->ss_gpio, 0);
    esp_err_t ret = spi_device_polling_transmit(ctx->spi_handle_write, &t);
    gpio_set_level(ctx->ss_gpio, 1);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed: %d", ret);
    }

    return ret;
}

void trf7962a_dump_regs(trf7962a_t ctx)
{
    ESP_LOGI(TAG, "Register dump");

    for (int reg = 0; reg < 0x20; reg++)
    {
        uint8_t val = 0;
        /* test - set registers to unusual values */
        trf7962a_set_reg(ctx, reg, ~reg);
        trf7962a_get_reg(ctx, reg, &val);
        ESP_LOGI(TAG, "   0x%02X: 0x%02X", reg, val);
    }
    ESP_LOGI(TAG, "Done");
}

trf7962a_t trf7962a_init(spi_host_device_t host_id, int gpio)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Initialize");

    trf7962a_t ctx = calloc(1, sizeof(struct trf7962a_s));

    ctx->ss_gpio = gpio;
    gpio_set_level(ctx->ss_gpio, 1);

    spi_device_interface_config_t devcfg_write = {
        .clock_speed_hz = 2 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = -1,
        .queue_size = 7};

    spi_device_interface_config_t devcfg_read = {
        .clock_speed_hz = 2 * 1000 * 1000,
        .mode = 1,
        .spics_io_num = -1,
        .queue_size = 7};

    esp_err_t ret;

    ret = spi_bus_add_device(host_id, &devcfg_write, &ctx->spi_handle_write);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed: %d", ret);
    }
    ret = spi_bus_add_device(host_id, &devcfg_read, &ctx->spi_handle_read);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed: %d", ret);
    }

    trf7962a_dump_regs(ctx);
    return ctx;
}

esp_err_t trf7962a_deinit(trf7962a_t ctx)
{
    return ESP_OK;
}



/* parts from
    https://github.com/electricimp/trf7962a/blob/master/trf7962a.device.lib.nut
*/

#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "esp_attr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "trf7962a.h"
#include "trf7962a_regs.h"

static const char *TAG = "TRF7962A";

esp_err_t trf7962a_get_reg(trf7962a_t ctx, uint8_t reg, uint8_t *data, int count)
{
    esp_err_t ret;

    gpio_set_level(ctx->ss_gpio, 0);

    spi_transaction_t t = {0};
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 8;

    t.tx_data[0] = (reg & 0b00011111) | REGISTER_B7 | READ_B6 | CONTINUOUS_MODE_REG_B5;
    ret = spi_device_polling_transmit(ctx->spi_handle_write, &t);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed: %d", ret);
    }

    uint8_t rx_buffer[32] = {0};
    t.tx_buffer = NULL;
    t.rx_buffer = rx_buffer;
    t.length = 8 * count;
    t.flags = 0;

    ret = spi_device_polling_transmit(ctx->spi_handle_read, &t);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed: %d", ret);
    }

    gpio_set_level(ctx->ss_gpio, 1);

    if (0)
    {
        for (int pos = 0; pos < t.length / 8; pos++)
        {
            ESP_LOGE(TAG, "    %02X", rx_buffer[pos]);
        }
    }
    memmove(&data[0], &rx_buffer[0], count);
    return ret;
}

esp_err_t trf7962a_set_reg(trf7962a_t ctx, uint8_t reg, uint8_t val)
{
    spi_transaction_t t = {0};

    t.flags |= SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 16;
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

esp_err_t trf7962a_set_mask(trf7962a_t ctx, uint8_t reg, uint8_t mask, uint8_t bits_to_set)
{
    uint8_t val = 0;

    if (mask)
    {
        trf7962a_get_reg(ctx, reg, &val, 1);
        val &= mask;
    }
    val |= bits_to_set;

    return trf7962a_set_reg(ctx, reg, val);
}

esp_err_t trf7962a_command(trf7962a_t ctx, uint8_t cmd)
{
    spi_transaction_t t = {0};

    t.flags |= SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.length = 16;
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

static uint8_t trf7962a_irq_status(trf7962a_t ctx)
{
    uint8_t status[2];
    trf7962a_get_reg(ctx, REG_IRQ_STATUS, status, 2);

    return status[0];
}

static uint8_t trf7962a_fifo_status(trf7962a_t ctx)
{
    uint8_t status;
    trf7962a_get_reg(ctx, REG_FIFO_STATUS, &status, 1);

    return status;
}

void trf7962a_irq_reset(trf7962a_t ctx)
{
    uint32_t dummy;

    trf7962a_irq_status(ctx);
    while (xQueueReceive(ctx->irq_received, &dummy, 0))
    {
    }
}

bool trf7962a_irq_check(trf7962a_t ctx)
{
    uint32_t dummy;
    ctx->irq_status = trf7962a_irq_status(ctx);
    if (!xQueueReceive(ctx->irq_received, &dummy, 0))
    {
        return false;
    }

    return true;
}

esp_err_t trf7962a_read_fifo(trf7962a_t ctx, uint8_t *data, uint8_t length)
{
    for (int pos = 0; pos < length; pos++)
    {
        trf7962a_get_reg(ctx, REG_FIFO, &data[pos], 3);
    }
    return ESP_OK;
}

int32_t trf7962a_write_fifo(trf7962a_t ctx, bool initiate, uint8_t *data, uint16_t length)
{
    uint8_t tx_buf[8 + TRF7962A_FIFO_SIZE];
    uint8_t rx_buf[8 + TRF7962A_FIFO_SIZE];
    int fifo_avail = 0;
    int tx_length = 0;

    /* when resetting the FIFO, we have 12 bytes available */
    if (initiate)
    {
        fifo_avail = TRF7962A_FIFO_SIZE;

        /* inform about total transfer size */
        tx_buf[tx_length++] = CMD_RESET_FIFO | COMMAND_B7 | WRITE_B6;
        tx_buf[tx_length++] = CMD_TRANSMIT_CRC | COMMAND_B7 | WRITE_B6;
        tx_buf[tx_length++] = REG_TX_LENGTH_BYTE_1 | REGISTER_B7 | WRITE_B6 | CONTINUOUS_MODE_REG_B5;
        tx_buf[tx_length++] = length >> 4;
        tx_buf[tx_length++] = length << 4;
    }
    else
    {
        /* else we have to read the FIFO status */
        uint8_t val = 0;

        trf7962a_get_reg(ctx, REG_FIFO_STATUS, &val, 1);
        fifo_avail = TRF7962A_FIFO_SIZE - (val & 0x0F);
        ESP_LOGI(TAG, " - continue with %d byte available, FIFO: 0x%02X", fifo_avail, val);

        /* only write to FIFO buffer */
        tx_buf[tx_length++] = REG_FIFO | REGISTER_B7 | WRITE_B6 | CONTINUOUS_MODE_REG_B5;
    }

    /* build the packet to send then */
    uint32_t xfer_size = (length < fifo_avail) ? length : fifo_avail;

    memcpy(&tx_buf[tx_length], data, xfer_size);
    tx_length += xfer_size;

    /* initiate the transaction */
    spi_transaction_t t = {0};
    t.length = tx_length * 8;
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;

    gpio_set_level(ctx->ss_gpio, 0);
    esp_err_t ret = spi_device_polling_transmit(ctx->spi_handle_write, &t);
    gpio_set_level(ctx->ss_gpio, 1);

    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "trf7962a_write_fifo failed: %d", ret);
        return ESP_FAIL;
    }

    return xfer_size;
}

esp_err_t trf7962a_write_packet(trf7962a_t ctx, uint8_t *data, uint8_t length)
{
    esp_err_t ret = ESP_FAIL;
    uint32_t sent = 0;

    trf7962a_reset(ctx);

    while (sent < length)
    {
        int32_t ret = trf7962a_write_fifo(ctx, sent == 0, &data[sent], length - sent);

        if (ret < 0)
        {
            ESP_LOGE(TAG, "Tx failed");
            return ESP_FAIL;
        }
        sent += ret;
    }

    int64_t t_before_us = esp_timer_get_time();
    while (true)
    {
        trf7962a_irq_check(ctx);

        if (ctx->irq_status & 0x1F)
        {
            ESP_LOGE(TAG, "Tx failed (reason 0x%02X)", ctx->irq_status & 0x1F);
            ret = ESP_FAIL;
            break;
        }
        /* wait till reception starts */
        if (ctx->irq_status & 0x40)
        {
            ESP_LOGE(TAG, "Response, IRQ 0x%02X, FIFO %02X", ctx->irq_status, trf7962a_fifo_status(ctx));
            ret = ESP_OK;
            break;
        }

        int64_t t_loop_us = esp_timer_get_time();
        if (t_loop_us - t_before_us > 100000)
        {
            ESP_LOGE(TAG, "Response timeout, IRQ 0x%02X, FIFO %02X", ctx->irq_status, trf7962a_fifo_status(ctx));
            ret = ESP_FAIL;
            break;
        }
    }
    return ret;
}

esp_err_t trf7962a_read_packet(trf7962a_t ctx, uint8_t *data, uint8_t *length)
{
    esp_err_t ret = ESP_FAIL;
    uint32_t timeout = 100000;
    *length = 0;

    int64_t t_before_us = esp_timer_get_time();
    while (true)
    {
        int64_t t_loop_us = esp_timer_get_time();
        if (t_loop_us - t_before_us > timeout)
        {
            /* hack. we are always reading one less than in buffer because this chip sucks.
               read that byte here */
            if(*length)
            {
                int avail = 1;
                trf7962a_read_fifo(ctx, &data[*length], avail);
                (*length) += avail;
            }
            ESP_LOGI(TAG, "Rx timed out, %d bytes read", *length);
            break;
        }

        trf7962a_irq_check(ctx);
        if (ctx->irq_status & 0x1F)
        {
            ESP_LOGE(TAG, "Rx failed (reason 0x%02X), %d bytes read", ctx->irq_status & 0x1F, *length);
            ret = ESP_FAIL;
            break;
        }

        /* as long there is some data */
        if (ctx->irq_status & 0x40)
        {
            uint8_t avail = ((trf7962a_fifo_status(ctx)) & 0x0F);

            if (avail >= TRF7962A_FIFO_SIZE)
            {
                ESP_LOGE(TAG, "Rx FIFO fill state %d", avail);
                break;
            }

            if (avail > 0)
            {
                ret = ESP_OK;
                // ESP_LOGI(TAG, "trf7962a_read_packet avail: %d IRQ: %02X, FIFO: %02X", avail, ctx->irq_status, trf7962a_fifo_status(ctx));
                t_before_us = esp_timer_get_time();
                timeout = 50000;
                trf7962a_read_fifo(ctx, &data[*length], avail);
                (*length) += avail;
                trf7962a_fifo_status(ctx);
            }
        }
    }

    return ret;
}

esp_err_t trf7962a_xmit(trf7962a_t ctx, uint8_t *tx_data, uint8_t tx_length, uint8_t *rx_data, uint8_t *rx_length)
{
    ESP_LOGI(TAG, "Tx %d bytes", tx_length);
    if (trf7962a_write_packet(ctx, tx_data, tx_length) != ESP_OK)
    {
        return ESP_FAIL;
    }
    if (trf7962a_read_packet(ctx, rx_data, rx_length) != ESP_OK)
    {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Rx finished, %d bytes read", *rx_length);

    return ESP_OK;
}

void trf7962a_dump_regs(trf7962a_t ctx)
{
    ESP_LOGI(TAG, "Register dump");

    for (int reg = 0; reg < 0x20; reg++)
    {
        uint8_t val = 0;

        trf7962a_get_reg(ctx, reg, &val, 1);
        ESP_LOGI(TAG, "   0x%02X: 0x%02X", reg, val);
    }
    ESP_LOGI(TAG, "Done");
}

void trf7962a_isr(void *ctx_in)
{
    trf7962a_t ctx = (trf7962a_t)ctx_in;
    uint32_t value = 0;
    xQueueSendFromISR(ctx->irq_received, &value, NULL);
}

void trf7962a_field(trf7962a_t ctx, bool enabled)
{
    trf7962a_set_mask(ctx, REG_CHIP_STATUS_CONTROL, ~0x20, enabled ? 0x20 : 0);
}

void trf7962a_reset(trf7962a_t ctx)
{
    /* reset chip */
    trf7962a_command(ctx, CMD_SOFT_INIT);
    trf7962a_command(ctx, CMD_IDLING);
    trf7962a_command(ctx, CMD_RESET_FIFO);

    /* register init */
    int pos = 0;
    uint8_t init_sequence[][3] = {TRF7962A_INIT_REGS};
    while (init_sequence[pos][0] != 0xFF)
    {
        trf7962a_set_mask(ctx, init_sequence[pos][0], init_sequence[pos][1], init_sequence[pos][2]);
        pos++;
    }

    trf7962a_irq_reset(ctx);
}

trf7962a_t trf7962a_init(spi_host_device_t host_id, int gpio)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Initialize");

    trf7962a_t ctx = calloc(1, sizeof(struct trf7962a_s));

    ctx->ss_gpio = gpio;
    ctx->irq_received = xQueueCreate(10, sizeof(uint32_t));

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

    trf7962a_reset(ctx);

    // trf7962a_dump_regs(ctx);
    return ctx;
}

esp_err_t trf7962a_deinit(trf7962a_t ctx)
{
    return ESP_OK;
}

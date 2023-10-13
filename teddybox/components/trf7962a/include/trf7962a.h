
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#include "esp_err.h"
#include "driver/spi_master.h"

    typedef struct trf7962a_s *trf7962a_t;

    struct trf7962a_s
    {
        spi_device_handle_t spi_handle_write;
        spi_device_handle_t spi_handle_read;
        int ss_gpio;
        QueueHandle_t irq_received;
        uint8_t irq_status;
    };

    enum ISO15693_RESULT
    {
        ISO15693_NO_RESPONSE = 0x00,
        ISO15693_VALID_RESPONSE = 0x01,
        ISO15693_INVALID_RESPONSE = 0x02,
        ISO15693_INVENTORY_NO_RESPONSE = 0x10,
        ISO15693_INVENTORY_VALID_RESPONSE = 0x11,
        ISO15693_INVENTORY_INVALID_RESPONSE = 0x12,
        ISO15693_GET_RANDOM_NO_RESPONSE = 0x20,
        ISO15693_GET_RANDOM_VALID = 0x21,
        ISO15693_GET_RANDOM_INVALID = 0x22,
        ISO15693_SET_PASSWORD_NO_RESPONSE = 0x30,
        ISO15693_SET_PASSWORD_CORRECT = 0x31,
        ISO15693_SET_PASSWORD_INCORRECT = 0x32,
        ISO15693_READ_SINGLE_BLOCK_NO_RESPONSE = 0x40,
        ISO15693_READ_SINGLE_BLOCK_VALID_RESPONSE = 0x41,
        ISO15693_READ_SINGLE_BLOCK_INVALID_RESPONSE = 0x42,
    };

    enum TRF_STATUS
    {
        STATUS_TRF_IDLE = 0x00,
        STATUS_TX_COMPLETE = 0x01,
        STATUS_RX_COMPLETE = 0x02,
        STATUS_TX_ERROR = 0x03,
        STATUS_RX_WAIT = 0x04,
        STATUS_RX_WAIT_EXTENSION = 0x05,
        STATUS_TX_WAIT = 0x06,
        STATUS_PROTOCOL_ERROR = 0x07,
        STATUS_COLLISION_ERROR = 0x08,
        STATUS_NO_RESPONSE_RECEIVED = 0x09,
        STATUS_NO_RESPONSE_RECEIVED_15693 = 0x0A
    };

    enum REG_CMD_WORD_BITS
    {
        COMMAND_B7 = 0b10000000,
        REGISTER_B7 = 0b00000000,
        READ_B6 = 0b01000000,
        WRITE_B6 = 0b00000000,
        CONTINUOUS_MODE_REG_B5 = 0b00100000
    };
    enum DIRECT_COMMANDS
    {
        CMD_IDLING = 0x00,
        CMD_SOFT_INIT = 0x03,
        CMD_RESET_FIFO = 0x0F,
        CMD_TRANSMIT_NO_CRC = 0x10,
        CMD_TRANSMIT_CRC = 0x11,
        CMD_TRANSMIT_DELAY_NO_CRC = 0x12,
        CMD_TRANSMIT_DELAY_CRC = 0x13,
        CMD_TRANSMT_NEXT_SLOT = 0x14,
        CMD_BLOCK_RECIEVER = 0x16,
        CMD_ENABLE_RECIEVER = 0x17,
        CMD_TEST_INTERNAL_RF = 0x18,
        CMD_TEST_EXTERNAL_RF = 0x19,
        CMD_RECEIVER_GAIN_ADJ = 0x1A
    };
    enum REGISTER
    {
        REG_CHIP_STATUS_CONTROL = 0x00,
        REG_ISO_CONTROL = 0x01,
        REG_ISO14443B_TX_OPTIONS = 0x02,
        REG_ISO14443A_BITRATE_OPTIONS = 0x03,
        REG_TX_TIMER_EPC_HIGH = 0x04,
        REG_TX_TIMER_EPC_LOW = 0x05,
        REG_TX_PULSE_LENGTH_CONTROL = 0x06,
        REG_RX_NO_RESPONSE_WAIT_TIME = 0x07,
        REG_RX_WAIT_TIME = 0x08,
        REG_MODULATOR_CONTROL = 0x09,
        REG_RX_SPECIAL_SETTINGS = 0x0A,
        REG_REGULATOR_CONTROL = 0x0B,
        REG_IRQ_STATUS = 0x0C,
        REG_IRQ_MASK = 0x0D,
        REG_COLLISION_POSITION = 0x0E,
        REG_RSSI_LEVELS = 0x0F,
        REG_SPECIAL_FUNCTION_1 = 0x10,
        REG_TEST_SETTINGS_1 = 0x1A,
        REG_TEST_SETTINGS_2 = 0x1B,
        REG_FIFO_STATUS = 0x1C,
        REG_TX_LENGTH_BYTE_1 = 0x1D,
        REG_TX_LENGTH_BYTE_2 = 0x1E,
        REG_FIFO = 0x1F
    };
    enum IRQ_STATUS
    {
        IRQ_IDLING = 0x00,
        IRQ_NO_RESPONSE = 0x01,
        IRQ_COLLISION_ERROR = 0x02,
        IRQ_FRAMING_ERROR = 0x04,
        IRQ_PARITY_ERROR = 0x08,
        IRQ_CRC_ERROR = 0x10,
        IRQ_FIFO_HIGH_OR_LOW = 0x20,
        IRQ_RX_COMPLETE = 0x40,
        IRQ_TX_COMPLETE = 0x80
    };

#define TRF7962A_FIFO_SIZE 12

#define TRF7962A_INIT_REGS                          \
    {REG_CHIP_STATUS_CONTROL, 0x00, 0x21},          \
    {REG_ISO_CONTROL, 0x00, 0x82},                  \
    {REG_IRQ_MASK, 0x00, 0x3E},                     \
    {REG_MODULATOR_CONTROL, 0x00, 0x21},            \
    {REG_TX_PULSE_LENGTH_CONTROL, 0x00, 0x80},      \
    {0xFF, 0xFF}

    /*
    
    {REG_RX_NO_RESPONSE_WAIT_TIME, 0x00, 0x14},     \
    {REG_RX_WAIT_TIME, 0x00, 0x1F},                 \
    {REG_RX_SPECIAL_SETTINGS, 0x0F, 0x40},          \
    {REG_SPECIAL_FUNCTION_1, 0xFF, 0x10},           \
    */

trf7962a_t trf7962a_init(spi_host_device_t host_id, int gpio);
esp_err_t trf7962a_get_reg(trf7962a_t ctx, uint8_t reg, uint8_t *val, int count);
esp_err_t trf7962a_set_reg(trf7962a_t ctx, uint8_t reg, uint8_t val);
esp_err_t trf7962a_command(trf7962a_t ctx, uint8_t cmd);
esp_err_t trf7962a_write_packet(trf7962a_t ctx, uint8_t *data, uint8_t length);
esp_err_t trf7962a_read_packet(trf7962a_t ctx, uint8_t *data, uint8_t *length);
void trf7962a_reset(trf7962a_t ctx);
void trf7962a_isr(void *ctx_in);
esp_err_t trf7962a_xmit(trf7962a_t ctx, uint8_t *tx_data, uint8_t tx_length, uint8_t *data_data, uint8_t *rx_length);
void trf7962a_field(trf7962a_t ctx, bool enabled);

#ifdef __cplusplus
}
#endif

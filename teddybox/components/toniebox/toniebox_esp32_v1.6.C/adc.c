
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_log.h"

#include "board.h"

#define TAG "[ADC]"

#define NO_OF_SAMPLES 32
#define ADC_FREQ 1000
#define ADC_RESULT_BYTE 4
#define ADC_CONV_LIMIT_EN 0
#define ADC_OUTPUT_TYPE ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define ADC_CONV_MODE ADC_CONV_SINGLE_UNIT_1

#define CHANNELS 2

static const int adc_channels[CHANNELS] = {7, 8};
static float adc_results[CHANNELS];
static adc_digi_pattern_config_t adc_pattern[CHANNELS] = {0};
static uint8_t adc_result_buffer[ADC_RESULT_BYTE * NO_OF_SAMPLES] = {0};

static void continuous_adc_init()
{
    adc_digi_init_config_t adc_dma_config = {
        .max_store_buf_size = sizeof(adc_result_buffer),
        .conv_num_each_intr = ADC_RESULT_BYTE * NO_OF_SAMPLES};

    for (int i = 0; i < CHANNELS; i++)
    {
        adc_dma_config.adc1_chan_mask |= BIT(adc_channels[i]);
    }

    ESP_ERROR_CHECK(adc_digi_initialize(&adc_dma_config));

    adc_digi_configuration_t dig_cfg = {
        .conv_limit_en = ADC_CONV_LIMIT_EN,
        .conv_limit_num = NO_OF_SAMPLES,
        .sample_freq_hz = ADC_FREQ,
        .conv_mode = ADC_CONV_MODE,
        .format = ADC_OUTPUT_TYPE,
        .pattern_num = CHANNELS};

    for (int i = 0; i < CHANNELS; i++)
    {
        adc_pattern[i].atten = ADC_ATTEN_DB_0;
        adc_pattern[i].channel = adc_channels[i];
        adc_pattern[i].unit = 0;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_digi_controller_configure(&dig_cfg));
}

void adc_update()
{
    uint32_t ret_num;
    esp_err_t ret = adc_digi_read_bytes(adc_result_buffer, sizeof(adc_result_buffer), &ret_num, 0);

    if (ret == ESP_OK || ret == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGD(TAG, "ret is %x, ret_num is %lu", ret, ret_num);
        for (int i = 0; i < ret_num; i += ADC_RESULT_BYTE)
        {
            adc_digi_output_data_t *p = (void *)&adc_result_buffer[i];

            for (int i = 0; i < CHANNELS; i++)
            {
                if (p->type2.channel == adc_channels[i])
                {
                    adc_results[i] = ((15 * adc_results[i]) + p->type2.data) / 16.0f;
                    ESP_LOGD(TAG, "ch%d val: %x avg: %2.2f", p->type2.channel, p->type2.data, adc_results[i]);
                    break;
                }
            }
        }
    }
}

float adc_get(int chan)
{
    adc_update();

    return adc_results[chan % CHANNELS];
}

void adc_init()
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Starting");
    continuous_adc_init();
    adc_digi_start();
    ESP_LOGI(TAG, "Done");
}

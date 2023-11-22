
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

#include "board.h"

#define TAG "[ADC]"

#define CHANNELS 2

static const int adc_channels[CHANNELS] = {7, 8};
static adc_oneshot_unit_handle_t adc1_handle;


float adc_get(int chan)
{ 
    int adc_raw;
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, adc_channels[chan], &adc_raw));

    return adc_raw * 3.3 / ((1<<12) -1);
}

void adc_init()
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Starting");
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_0,
    };
    
    for (int i = 0; i < CHANNELS; i++)
    {
        ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, adc_channels[i], &config));
    }
}

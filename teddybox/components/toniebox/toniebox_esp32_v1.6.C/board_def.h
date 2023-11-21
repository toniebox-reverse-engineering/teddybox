/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2022 <ESPRESSIF SYSTEMS (SHANGHAI) CO., LTD>
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

#ifndef _AUDIO_BOARD_DEFINITION_H_
#define _AUDIO_BOARD_DEFINITION_H_


extern audio_hal_func_t AUDIO_CODEC_DAC3100_DEFAULT_HANDLE;

#define AUDIO_CODEC_DEFAULT_CONFIG(){                   \
        .adc_input  = AUDIO_HAL_ADC_INPUT_LINE1,        \
        .dac_output = AUDIO_HAL_DAC_OUTPUT_ALL,         \
        .codec_mode = AUDIO_HAL_CODEC_MODE_BOTH,        \
        .i2s_iface = {                                  \
            .mode = AUDIO_HAL_MODE_SLAVE,               \
            .fmt = AUDIO_HAL_I2S_NORMAL,                \
            .samples = AUDIO_HAL_48K_SAMPLES,           \
            .bits = AUDIO_HAL_BIT_LENGTH_16BITS,        \
        },                                              \
};

/**
 * @brief SDCARD Function Definition
 *        PMOD2 for one line sdcard
 * required by sdcard.c 
 */
#define SDCARD_OPEN_FILE_NUM_MAX    5
#define ESP_SD_PIN_CLK              GPIO_NUM_35
#define ESP_SD_PIN_CMD              GPIO_NUM_38
#define ESP_SD_PIN_D0               GPIO_NUM_36
#define ESP_SD_PIN_D1               GPIO_NUM_37
#define ESP_SD_PIN_D2               GPIO_NUM_33
#define ESP_SD_PIN_D3               GPIO_NUM_34
#define ESP_SD_PIN_D4               -1
#define ESP_SD_PIN_D5               -1
#define ESP_SD_PIN_D6               -1
#define ESP_SD_PIN_D7               -1
#define ESP_SD_PIN_CD               -1
#define ESP_SD_PIN_WP               -1

/* required by codec drivers, even if not selected */
#define BOARD_PA_GAIN -1

/* GPIO definitions 
   https://github.com/toniebox-reverse-engineering/toniebox/blob/master/wiki/Toniebox-ESP32-Pinout.md 
*/

#define SPI_SS_GPIO                 GPIO_NUM_1
#define SPI_MOSI_GPIO               GPIO_NUM_2
#define SPI_MISO_GPIO               GPIO_NUM_3
#define SPI_SCLK_GPIO               GPIO_NUM_4
#define I2C_SDA_GPIO                GPIO_NUM_5
#define I2C_SCL_GPIO                GPIO_NUM_6
#define WAKEUP_GPIO                 GPIO_NUM_7
#define ADC_CHARG_GPIO              GPIO_NUM_8
#define ADC_VBATT_GPIO              GPIO_NUM_9
#define I2S_DATA_GPIO               GPIO_NUM_10
#define I2S_BCK_GPIO                GPIO_NUM_11
#define I2S_WS_GPIO                 GPIO_NUM_12
#define TRF7962A_IRQ_GPIO           GPIO_NUM_13
#define LIS3DH_IRQ_GPIO             GPIO_NUM_14

#define LED_BLUE_GPIO               GPIO_NUM_17
#define LED_GREEN_GPIO              GPIO_NUM_18
#ifdef DEVBOARD
#define LED_RED_GPIO                GPIO_NUM_18
#define EAR_BIG_GPIO                GPIO_NUM_21
#else
#define LED_RED_GPIO                GPIO_NUM_19
#define EAR_BIG_GPIO                GPIO_NUM_20
#endif

#define EAR_SMALL_GPIO              GPIO_NUM_21

#define DAC3100_RESET_GPIO          GPIO_NUM_26

#define POWER_GPIO                  GPIO_NUM_45
#define SD_POWER_GPIO               GPIO_NUM_47
#define HEADPHONE_DETECT            GPIO_NUM_48


#endif

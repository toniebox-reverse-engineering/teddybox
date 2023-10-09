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

#ifndef __NEW_CODEC_H__
#define __NEW_CODEC_H__

#include "audio_hal.h"

#ifdef __cplusplus
extern "C" {
#endif


#define DAC3100_ADDR (0x18 << 1)

        enum PAGE {
            SERIAL_IO = 0x00,
            DAC_OUT_VOL = 0x01,
            MCLK_DIVIDER = 0x03,
            DAC_FILTER_DRC_COE_1A = 0x08,
            DAC_FILTER_DRC_COE_2A = 0x09,
            DAC_FILTER_DRC_COE_1B = 0x0C,
            DAC_FILTER_DRC_COE_2B = 0x0D,
        };
        enum ADDR {
            PAGE_CONTROL = 0x00,
        };

        enum ADDR_P0_SERIAL {
            SOFTWARE_RESET = 0x01,
            CLOCKGEN_MUX = 0x04,
            PLL_P_R_VAL = 0x05,
            PLL_J_VAL = 0x06,
            PLL_D_VAL_MSB = 0x07,
            PLL_D_VAL_LSB = 0x08,
            DAC_NDAC_VAL = 0x0B,
            DAC_MDAC_VAL = 0x0C,
            DAC_DOSR_VAL_MSB = 0x0D,
            DAC_DOSR_VAL_LSB = 0x0E,
            CODEC_IF_CTRL1 = 0x1B,
            DAC_FLAG_REG1 = 0x25,
            DAC_FLAG_REG2 = 0x26,
            DAC_INTR_FLAGS = 0x2C,
            INTR_FLAGS = 0x2E,
            INT1_CTRL_REG = 0x30,
            GPIO1_INOUT_CTRL = 0x33,
            DAC_PROC_BLOCK_SEL = 0x3C,
            DAC_DATA_PATH_SETUP = 0x3F,
            DAC_VOL_CTRL = 0x40,
            DAC_VOL_L_CTRL = 0x41,
            DAC_VOL_R_CTRL = 0x42,
            HEADSET_DETECT = 0x43,
            BEEP_L_GEN = 0x47,
            BEEP_R_GEN = 0x48,
            BEEP_LEN_MSB = 0x49,
            BEEP_LEN_MID = 0x4A,
            BEEP_LEN_LSB = 0x4B,
            BEEP_SIN_MSB = 0x4C,
            BEEP_SIN_LSB = 0x4D,
            BEEP_COS_MSB = 0x4E,
            BEEP_COS_LSB = 0x4F,
            VOL_MICDET_SAR_ADC = 0x74,
        };
        enum ADDR_P1_DAC_OUT {
            HP_DRIVERS = 0x1F,
            SPK_AMP = 0x20,
            HP_OUT_POP_REM_SET = 0x21,
            OUT_PGA_RAMP_DOWN_PER_CTRL = 0x22,
            DAC_LR_OUT_MIX_ROUTING = 0x23,
            L_VOL_TO_HPL = 0x24,
            R_VOL_TO_HPR = 0x25,
            L_VOL_TO_SPK = 0x26,
            HPL_DRIVER = 0x28,
            HPR_DRIVER = 0x29,
            SPK_DRIVER = 0x2A,
            HP_DRIVER_CTRL = 0x2C,
            MICBIAS = 0x2E,
        };
        enum ADDR_P3_MCLK {
            TIMER_CLK_MCLK_DIV = 0x10,
        };

/**
 * @brief Initialize dac3100 chip
 *
 * @param cfg configuration of dac3100
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t dac3100_init(audio_hal_codec_config_t *cfg);

/**
 * @brief Deinitialize dac3100 chip
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t dac3100_deinit(void);

/**
 * The functions dac3100_ctrl_state and dac3100_config_i2s are not used by this driver.
 * They are kept here to maintain the uniformity and convenience of the interface
 * of the ADF project.
 * These settings for dac3100 are burned in firmware and configuration files.
 * Default i2s configuration: 48000Hz, 16bit, Left-Right channels.
 * Use resampling to be compatible with different file types.
 *
 * @brief Control dac3100 chip
 *
 * @param mode codec mode
 * @param ctrl_state start or stop decode or encode progress
 *
 * @return
 *     - ESP_FAIL Parameter error
 *     - ESP_OK   Success
 */
esp_err_t dac3100_ctrl_state(audio_hal_codec_mode_t mode, audio_hal_ctrl_t ctrl_state);

/**
 * @brief Configure dac3100 codec mode and I2S interface
 *
 * @param mode codec mode
 * @param iface I2S config
 *
 * @return
 *     - ESP_FAIL Parameter error
 *     - ESP_OK   Success
 */
esp_err_t dac3100_config_i2s(audio_hal_codec_mode_t mode, audio_hal_codec_i2s_iface_t *iface);

/**
 * @brief mute or unmute the codec
 *
 * @param mute:  true, false
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t dac3100_set_mute(bool mute);

/**
 * @brief  Set volume
 *
 * @param volume:  volume (0~100)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t dac3100_set_volume(int volume);

/**
 * @brief Get volume
 *
 * @param[out] *volume:  volume (0~100)
 *
 * @return
 *     - ESP_OK
 *     - ESP_FAIL
 */
esp_err_t dac3100_get_volume(int *volume);


esp_err_t dac3100_set_gain(int gain);

#ifdef __cplusplus
}
#endif

#endif

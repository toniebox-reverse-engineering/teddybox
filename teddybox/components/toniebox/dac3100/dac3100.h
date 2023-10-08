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

#ifdef __cplusplus
}
#endif

#endif

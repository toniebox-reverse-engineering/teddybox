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
static i2c_bus_handle_t i2c_handle;
static uint8_t reg_cache[14][256];
static uint8_t reg_page = 0;

uint16_t ofwButtonFreqTable[5][4][2] = {
    {
        // 16000
        {0x278A, 0x79BD}, //+
        {0x30F9, 0x763F}, //++
        {0x18F5, 0x7D87}, //-
        {0x0F0A, 0x7F1A}  //--
    },
    {
        // 22050
        {0x1CEA, 0x7CB1}, //+
        {0x23F9, 0x7AD5}, //++
        {0x122A, 0x7EB2}, //-
        {0x0AED, 0x7F87}  //--
    },
    {
        // 32000
        {0x1404, 0x7E6D}, //+
        {0x18F7, 0x7D8A}, //++
        {0x0C8A, 0x7F61}, //-
        {0x0788, 0x7FC7}  //--
    },
    {
        // 44100
        {0x0E8D, 0x7F2B}, //+
        {0x122C, 0x7EB4}, //++
        {0x091B, 0x7FAC}, //-
        {0x0578, 0x7FE2}  //--
    },
    {
        // 48000
        {0x0D60, 0x7F4D}, //+
        {0x10B4, 0x7EE7}, //++
        {0x085E, 0x7FB9}, //-
        {0x0506, 0x7FE6}  //--
    }};

audio_hal_func_t AUDIO_CODEC_DAC3100_DEFAULT_HANDLE = {
    .audio_codec_initialize = dac3100_init,
    .audio_codec_deinitialize = dac3100_deinit,
    .audio_codec_ctrl = dac3100_ctrl_state,
    .audio_codec_config_iface = dac3100_config_i2s,
    .audio_codec_set_mute = dac3100_set_mute,
    .audio_codec_set_volume = dac3100_set_volume,
    .audio_codec_get_volume = dac3100_get_volume,
};

static esp_err_t dac3100_write_reg(uint8_t reg_add, uint8_t data)
{
    if (reg_add == 0)
    {
        reg_page = data;
    }
    else
    {
        reg_cache[reg_page][reg_add] = data;
    }
#ifdef DEVBOARD
    return ESP_OK;
#endif
    return i2c_bus_write_bytes(i2c_handle, DAC3100_ADDR, &reg_add, sizeof(reg_add), &data, sizeof(data));
}

static esp_err_t dac3100_read_reg(uint8_t reg_add, uint8_t *p_data)
{
#ifdef DEVBOARD
    *p_data = 0;
    return ESP_OK;
#endif
    esp_err_t err = i2c_bus_read_bytes(i2c_handle, DAC3100_ADDR, &reg_add, sizeof(reg_add), p_data, 1);

    if (err == ESP_OK)
    {
        reg_cache[reg_page][reg_add] = *p_data;
    }

    return err;
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

esp_err_t dac3100_dump_reg(enum PAGE page, uint8_t reg)
{
    uint8_t val = 0;
    dac3100_write_reg(PAGE_CONTROL, page);
    esp_err_t ret = dac3100_read_reg(reg, &val);
    ESP_LOGW(TAG, "  %x: %x", reg, val);

    return ret;
}

uint8_t dac3100_headset_detected()
{
    uint8_t val = 0;
    dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);
    /* reset flags  */
    dac3100_read_reg(DAC_INTR_FLAGS, &val);
    dac3100_read_reg(HEADSET_DETECT, &val);

    return (val >> 5) & 0x03;
}

esp_err_t dac3100_beep_generate(uint16_t sin, uint16_t cos, uint32_t length)
{
    dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);

    dac3100_write_reg(BEEP_LEN_MSB, length >> 16);
    dac3100_write_reg(BEEP_LEN_MID, length >> 8);
    dac3100_write_reg(BEEP_LEN_LSB, length);

    dac3100_write_reg(BEEP_SIN_MSB, sin >> 8);
    dac3100_write_reg(BEEP_SIN_LSB, sin);

    dac3100_write_reg(BEEP_COS_MSB, cos >> 8);
    dac3100_write_reg(BEEP_COS_LSB, cos);

    dac3100_write_reg(BEEP_L_GEN, 0x80);

    return ESP_OK;
}

esp_err_t dac3100_beep(uint16_t index, uint32_t length)
{
    uint16_t(*pBeep)[2] = ofwButtonFreqTable[4];

    return dac3100_beep_generate(pBeep[index][0], pBeep[index][1], length);
}

esp_err_t dac3100_init(audio_hal_codec_config_t *cfg)
{
    ESP_LOGI(TAG, "dac3100 init");

    i2c_handle = audio_board_get_handle()->i2c_handle;

    /* from datasheet */

    dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);
    dac3100_write_reg(SOFTWARE_RESET, 0x01);
    dac3100_write_reg(CLOCKGEN_MUX, 0x07);
    // dac3100_write_reg(PLL_J_VAL, 0x08);
    // dac3100_write_reg(PLL_D_VAL_MSB, 0x00);
    // dac3100_write_reg(PLL_D_VAL_LSB, 0x00);
    // dac3100_write_reg(PLL_P_R_VAL, 0x91);
    // dac3100_write_reg(DAC_NDAC_VAL, 0x88);
    // dac3100_write_reg(DAC_MDAC_VAL, 0x82);
    // dac3100_write_reg(DAC_DOSR_VAL_MSB, 0x00);
    // dac3100_write_reg(DAC_DOSR_VAL_LSB, 0x80);
    // dac3100_write_reg(CODEC_IF_CTRL1, 0x00);
    // dac3100_write_reg(DAC_PROC_BLOCK_SEL, 0x0B);

    // dac3100_write_reg(PAGE_CONTROL, DAC_FILTER_DRC_COE_1A);
    // dac3100_write_reg(0x01, 0x04);

    // dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);
    // dac3100_write_reg(VOL_MICDET_SAR_ADC, 0x00);

    dac3100_write_reg(PAGE_CONTROL, DAC_OUT_VOL);
    dac3100_write_reg(HP_DRIVERS, 0x04);
    // dac3100_write_reg(HP_OUT_POP_REM_SET, 0x4E);
    // dac3100_write_reg(DAC_LR_OUT_MIX_ROUTING, 0x44);
    dac3100_write_reg(HPL_DRIVER, 0x06);
    dac3100_write_reg(HPR_DRIVER, 0x06);
    dac3100_write_reg(SPK_DRIVER, 0x1C);
    dac3100_write_reg(HP_DRIVERS, 0xC4);
    dac3100_write_reg(SPK_AMP, 0x86);
    dac3100_write_reg(L_VOL_TO_HPL, 0x92);
    dac3100_write_reg(R_VOL_TO_HPR, 0x92);
    dac3100_write_reg(L_VOL_TO_SPK, 0x92);

    // dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);
    // dac3100_write_reg(DAC_DATA_PATH_SETUP, 0xD4);
    // dac3100_write_reg(DAC_VOL_CTRL, 0x00);

    /* https://github.com/toniebox-reverse-engineering/RvX_TLV320DAC3100/blob/master/RvX_TLV320DAC3100.cpp */
    dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);
    // dac3100_write_reg(SOFTWARE_RESET, 0x01);
    dac3100_write_reg(CLOCKGEN_MUX, 0x07);     // 0000:reserved, 01:PLL_CLKIN=BCLK, 11:CODEC_CLKIN=PLL_CLK
    dac3100_write_reg(PLL_J_VAL, 0x20);        // 00:reserved, 100000:PLL multiplier J=32 (0x20)
    dac3100_write_reg(PLL_D_VAL_MSB, 0x00);    // 00:reserved, 000000:fraktional multiplier D-value = 0
    dac3100_write_reg(PLL_D_VAL_LSB, 0x00);    // 00:reserved, 000000:fraktional multiplier D-value = 0
    dac3100_write_reg(PLL_P_R_VAL, 0x96);      // 1:PLL is power up, 001:PLL divider P=1, 110:PLL multiplier R=6
    dac3100_write_reg(DAC_NDAC_VAL, 0x84);     // 1:NDAC divider powered up, 0000100:DAC NDAC divider=4
    dac3100_write_reg(DAC_MDAC_VAL, 0x86);     // 1:MDAC divider powered up, 0000100:DAC MDAC divider=6
    dac3100_write_reg(DAC_DOSR_VAL_MSB, 0x01); // 000000:reserved, 01:DAC OSR MSB =256
    dac3100_write_reg(DAC_DOSR_VAL_LSB, 0x00); // 00000000:DAC OSR LSB

    vTaskDelay(10 / portTICK_RATE_MS);

    dac3100_write_reg(CODEC_IF_CTRL1, 0x00); // 00:Codec IF=I2S, 00: Codec IF WL=16 bits, 0:BCLK=Input, 0:WCKL=Output, 0:reserved        // w IF statt INT

    dac3100_write_reg(DAC_PROC_BLOCK_SEL, 0x19); // 000:reserved, 11001:DAC signal-processing block PRB_P25

    dac3100_write_reg(PAGE_CONTROL, DAC_OUT_VOL);
    dac3100_write_reg(HP_OUT_POP_REM_SET, 0x4E);         // 0:simultan.DAC/HP/SP, 1001:power-on-time=1.22s*,11:drv.ramp-up=3.9ms,0:CM voltage
    dac3100_write_reg(OUT_PGA_RAMP_DOWN_PER_CTRL, 0x70); // 0:reserved, 111=30.5ms*, 0000:reserved *8.2MHz
    dac3100_write_reg(DAC_LR_OUT_MIX_ROUTING, 0x44);     // 01:DAC_L to MixAmp_L,00:AIN1/2 not routed, 01:DAC_R to MIxAmp_R, 00:AIN1/2 not routed
    dac3100_write_reg(MICBIAS, 0x0B);                    // 0:SwPowDwn not enabled, 000:reserved, 1:MICBIAS powered up, 0:reserved, 11:MICBIAS=AVDD
    dac3100_write_reg(HP_DRIVER_CTRL, 0xE0);             // 000:Debounce Time=0us, 01:DAC perform.increased, 1:HPL output=lineout, 1:HPR output=lineout, 0:reserved ??? LINE

    dac3100_write_reg(PAGE_CONTROL, MCLK_DIVIDER);
    dac3100_write_reg(TIMER_CLK_MCLK_DIV, 0x01); // 0:Internal oscillator for delay timer, 0000001: MCLK divider=1

    dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);
    dac3100_write_reg(HEADSET_DETECT, 0x8C);   // 1:Headset detection enabled, RR, 011:Debounce Prog.Glitch=128ms, 00:Debounce Prog.Glitch=0ms
    dac3100_write_reg(INT1_CTRL_REG, 0x80);    // 1:Headset-insertion detect IRQ INT1, 0:Button-press detect, ...., 0=INT1 is only one pulse 2ms
    dac3100_write_reg(GPIO1_INOUT_CTRL, 0x14); // XX:reserved, 0101:GPIO1=INT1 output, X=GPIO1 input buffer value, GPIO1 Output=X

#if 0
    dac3100_write_reg(PAGE_CONTROL, DAC_OUT_VOL); // MUTE ALL
    dac3100_write_reg(L_VOL_TO_HPL, 0x7F);        // HPL Vol -oo
    dac3100_write_reg(R_VOL_TO_HPR, 0x7F);        // HPL Vol -oo
    dac3100_write_reg(L_VOL_TO_SPK, 0x7F);        // SPK Vol -oo
    vTaskDelay(50 / portTICK_RATE_MS);

    // MUTE HP Driver AND SPK Driver
    dac3100_write_reg(HPL_DRIVER, 0x02); // HPL driver is muted ??? must 1
    dac3100_write_reg(HPR_DRIVER, 0x02); // HPR driver is muted ??? must 1
    dac3100_write_reg(SPK_DRIVER, 0x00); // SPK driver is muted

    // PAUSE 50ms
    vTaskDelay(50 / portTICK_RATE_MS);

    // AMPS Power Down
    dac3100_write_reg(HP_DRIVERS, 0x00); // HPL HPR Driver Power Down  ??? must 1
    dac3100_write_reg(SPK_AMP, 0x06); // SPK Amp Power Down ??? must 000011

    // PAUSE 50ms
    vTaskDelay(50 / portTICK_RATE_MS);

    dac3100_write_reg(HPL_DRIVER, 0x06); // HPL driver 0dB, not muted
    dac3100_write_reg(HPR_DRIVER, 0x06); // HPR drvier 0dB, not muted
    dac3100_write_reg(HP_DRIVERS, 0xC4);   // HPL HPR is power up, 1,35V, Shortcut=Error ??? must 1
    dac3100_write_reg(L_VOL_TO_HPL, 0x92); // ??? Aux to HP ???
    dac3100_write_reg(R_VOL_TO_HPR, 0x92); // ??? Aux to HP ???

    dac3100_write_reg(L_VOL_TO_SPK, 0x00); // !!! FEHLTE !!!
    dac3100_write_reg(SPK_AMP, 0x86);      // !!! FEHLTE !!! SPK Amp Power Up

    // PAUSE 50ms
    vTaskDelay(50 / portTICK_RATE_MS);

#endif
    dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);
    dac3100_write_reg(DAC_DATA_PATH_SETUP, 0xD5); // DAC power on, Left=left, Right=Right, DAC Softstep HP STEREO

    dac3100_write_reg(DAC_VOL_L_CTRL, 0xDC);
    dac3100_write_reg(DAC_VOL_R_CTRL, 0xDC);

    // dac3100_write_reg(PAGE_CONTROL, DAC_OUT_VOL);
    // dac3100_write_reg(L_VOL_TO_SPK, 0x80);
    dac3100_write_reg(DAC_VOL_CTRL, 0x00);

    dac3100_set_gain(1);
    dac3100_set_mute(true);

    return ESP_OK;
}

esp_err_t dac3100_deinit(void)
{
    dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);
    dac3100_write_reg(SOFTWARE_RESET, 0x01);
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
    uint8_t reg_val = (reg_cache[1][0x2A] & ~0x04) | (mute ? 0 : 0x04);

    dac3100_write_reg(PAGE_CONTROL, DAC_OUT_VOL);
    dac3100_write_reg(SPK_DRIVER, reg_val);
    return ESP_OK;
}

esp_err_t dac3100_set_gain(int gain)
{
    uint8_t reg_val = (reg_cache[1][0x2A] & ~0x18) | (gain << 3);

    dac3100_write_reg(PAGE_CONTROL, DAC_OUT_VOL);
    dac3100_write_reg(SPK_DRIVER, reg_val);
    return ESP_OK;
}

esp_err_t dac3100_set_volume(int volume)
{
    int value = -635 + (volume * 635 / 100);
    uint8_t reg_val = value / 5;
    int beep_value = ((100 - volume) * 0x3F / 100);
    uint8_t reg_beep = beep_value & 0x3F;

    dac3100_write_reg(PAGE_CONTROL, SERIAL_IO);
    dac3100_write_reg(DAC_VOL_L_CTRL, reg_val);
    dac3100_write_reg(DAC_VOL_R_CTRL, reg_val);
    dac3100_write_reg(BEEP_R_GEN, 0x40 | (reg_beep));
    return ESP_OK;
}

esp_err_t dac3100_get_volume(int *volume)
{
    return ESP_OK;
}

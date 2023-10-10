/* Play MP3 file from SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include "esp_partition.h"

#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "board.h"
#include "playback.h"

static const char *TAG = "[TB]";

static wl_handle_t s_test_wl_handle;
void dir_list(const char *path)
{
    DIR *dir = opendir(path);
    while (true)
    {
        struct dirent *de = readdir(dir);
        if (!de)
        {
            break;
        }
        ESP_LOGI(TAG, "  - %s", de->d_name);
    }
    closedir(dir);
}
void dir_play(const char *path)
{
    DIR *dir = opendir(path);
    while (true)
    {
        struct dirent *de = readdir(dir);
        if (!de)
        {
            break;
        }
        ESP_LOGI(TAG, "  - %s", de->d_name);
    }
    closedir(dir);
}

void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 1.0 ] Board init");
    audio_board_handle_t board_handle = audio_board_init();

    ESP_LOGI(TAG, "[ 1.1 ] Mount sdcard");
    audio_board_sdcard_init(set, SD_MODE_4_LINE);

    dir_list("/sdcard");

    ESP_LOGI(TAG, "[ 1.2 ] Mount assets");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5};
    esp_vfs_fat_spiflash_mount("/spiflash", NULL, &mount_config, &s_test_wl_handle);

    dir_list("/spiflash");

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_hal_ctrl_codec(board_handle->audio_hal, AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Start playback handler");
    pb_init(set);

    int volume = 30;
    audio_hal_set_volume(audio_board_get_hal(), volume);

    ESP_LOGI(TAG, "[ 4 ] play startup sound");

    pb_play_default(CONTENT_DEFAULT_STARTUP);
    vTaskDelay(5000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "[ 5 ] play audio");
    if(pb_play_content(0x07BAC90F) != ESP_OK)
    {
        pb_play_default(CONTENT_DEFAULT_CODE_KOALA);
    }

    bool ear_big_prev = false;
    bool ear_small_prev = false;

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        bool ear_big = audio_board_ear_big();
        bool ear_small = audio_board_ear_small();

        if (ear_big && !ear_big_prev)
        {
            if (volume < 100)
            {
                ESP_LOGI(TAG, "Volume up");
                volume += 10;
                audio_hal_set_volume(audio_board_get_hal(), volume);
            }
        }

        if (ear_small && !ear_small_prev)
        {
            if (volume >= 10)
            {
                ESP_LOGI(TAG, "Volume down");
                volume -= 10;
                audio_hal_set_volume(audio_board_get_hal(), volume);
            }
        }

        ear_big_prev = ear_big;
        ear_small_prev = ear_small;
    }

    /* Stop all periph before removing the listener */
    esp_periph_set_stop_all(set);
    /* Release all resources */
    esp_periph_set_destroy(set);
}

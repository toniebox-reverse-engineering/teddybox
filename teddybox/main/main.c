/* Play MP3 file from SD Card

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "soc/rtc_cntl_reg.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include "esp_partition.h"
#include "esp_event.h"
#include "esp_sleep.h"

#include "esp_peripherals.h"
#include "periph_sdcard.h"
#include "board.h"
#include "playback.h"
#include "wifi.h"
#include "webserver.h"
#include "accel.h"
#include "dac3100.h"
#include "ota.h"
#include "nfc.h"
#include "cloud.h"
#include "ledman.h"

#include "config.h"

#include "esp_heap_trace.h"

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

void print_all_tasks(void *params)
{
    TaskStatus_t *taskStatusArray;
    UBaseType_t taskCount;
    UBaseType_t i;
    size_t free_heap_size;

    while (1)
    {
        taskCount = uxTaskGetNumberOfTasks();
        taskStatusArray = pvPortMalloc(taskCount * sizeof(TaskStatus_t));

        if (taskStatusArray != NULL)
        {
            taskCount = uxTaskGetSystemState(taskStatusArray, taskCount, NULL);

            free_heap_size = esp_get_free_heap_size();
            ESP_LOGI(TAG, "Task Count:     %d", taskCount);
            ESP_LOGI(TAG, "Free Heap Size: %d", free_heap_size);
            ESP_LOGI(TAG, "");
            ESP_LOGI(TAG, "StackMark Name");

            for (i = 0; i < taskCount; i++)
            {
                ESP_LOGI(TAG, "%6d %s",
                         uxTaskGetStackHighWaterMark(taskStatusArray[i].xHandle),
                         taskStatusArray[i].pcTaskName);
            }

            vPortFree(taskStatusArray);
        }

        vTaskDelay(pdMS_TO_TICKS(5000)); // Wait for 5 seconds
    }
}

void app_main(void)
{
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_log_level_set("*", ESP_LOG_WARN);
    esp_log_level_set(TAG, ESP_LOG_INFO);

    esp_periph_config_t periph_cfg = DEFAULT_ESP_PERIPH_SET_CONFIG();
    esp_periph_set_handle_t set = esp_periph_set_init(&periph_cfg);

    ESP_LOGI(TAG, "[ 1.0 ] Board init");
    audio_board_handle_t board_handle = audio_board_init();
    ledman_init();

    ESP_LOGI(TAG, "[ 1.1 ] Mount sdcard");
    audio_board_sdcard_init(set, SD_MODE_4_LINE);

    ESP_LOGI(TAG, "[ 1.2 ] Mount assets");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3};
    esp_vfs_fat_spiflash_mount("/spiflash", NULL, &mount_config, &s_test_wl_handle);

    ESP_LOGI(TAG, "[ 2 ] Start codec chip");
    audio_hal_ctrl_codec(audio_board_get_hal(), AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);

    ESP_LOGI(TAG, "[ 3 ] Start handlers");

    pb_init(set);

    // xTaskCreate(print_all_tasks, "print_all_tasks", 4096, NULL, 5, NULL);

    int volume = 30;
    audio_hal_set_volume(audio_board_get_hal(), volume);

    ESP_LOGI(TAG, "[ 4 ] detect headset");
    dac3100_set_mute(true);

    ESP_LOGI(TAG, "[ 5 ] play startup sound");
    pb_play_default(CONTENT_DEFAULT_STARTUP);

    bool ear_big_prev = false;
    bool ear_small_prev = false;

    accel_init(board_handle);
    wifi_init();
    nfc_init();
    cloud_init();
    /* already too much memory consumption, do not enable by default */
    // www_init();
    ota_init();

    int64_t last_activity_time = esp_timer_get_time();

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
                dac3100_beep(0, 0x140);
            }
            else
            {
                ESP_LOGI(TAG, "Volume up (limit)");
                dac3100_beep(1, 0x140);
            }
        }

        if (ear_small && !ear_small_prev)
        {
            if (volume >= 10)
            {
                ESP_LOGI(TAG, "Volume down");
                volume -= 10;
                audio_hal_set_volume(audio_board_get_hal(), volume);
                dac3100_beep(2, 0x140);
            }
            else
            {
                ESP_LOGI(TAG, "Volume down (limit)");
                dac3100_beep(3, 0x140);
            }
        }

        if (pb_is_playing() || ear_big || ear_small)
        {
            last_activity_time = esp_timer_get_time();
        }

        if ((esp_timer_get_time() - last_activity_time) > POWEROFF_TIMEOUT)
        {
            break;
        }

        ear_big_prev = ear_big;
        ear_small_prev = ear_small;
    }

    ledman_change("poweroff");
    audio_board_sdcard_unmount();

    ESP_LOGI(TAG, "Poweroff");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    audio_board_poweroff();

    ESP_LOGE(TAG, "back, quite unexpected...");
}

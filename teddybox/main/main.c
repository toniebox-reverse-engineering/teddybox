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
#include "driver/rtc_io.h"
#include "soc/rtc.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_timer.h"
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

typedef struct
{
    uint32_t rtc_magic;
    uint32_t volume;
    uint64_t nfc_uid;
    uint32_t play_position;
    uint32_t rtc_check;
} rtc_mem_t;

static RTC_DATA_ATTR rtc_mem_t rtc_storage;

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

static uint32_t rotl32a(uint32_t x, uint32_t n)
{
    return (x << n) | (x >> (32 - n));
}

static uint32_t rtc_checksum_calc()
{
    uint8_t *buf = (uint8_t *)&rtc_storage;
    uint32_t len = sizeof(rtc_storage) - sizeof(uint32_t);
    uint32_t chk = 0x55AA5A5A;

    for (int pos = 0; pos < len; pos++)
    {
        chk ^= buf[pos];
        chk = rotl32a(chk, 3);
        chk += buf[pos];
        chk = rotl32a(chk, 7);
    }
    return chk;
}

static void rtc_checksum_update()
{
    rtc_storage.rtc_check = rtc_checksum_calc();
}

static bool rtc_checksum_valid()
{
    return rtc_storage.rtc_check == rtc_checksum_calc();
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

    ESP_LOGI(TAG, "Board init");
    audio_board_handle_t board_handle = audio_board_init();
    ledman_init();

    ESP_LOGI(TAG, "Mount sdcard");
#ifndef DEVBOARD
    audio_board_sdcard_init(set, SD_MODE_4_LINE);
#endif

    ESP_LOGI(TAG, "Mount assets");
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 3};
    esp_vfs_fat_spiflash_mount("/spiflash", NULL, &mount_config, &s_test_wl_handle);

    ESP_LOGI(TAG, "Start codec chip");
#ifndef DEVBOARD
    audio_hal_ctrl_codec(audio_board_get_hal(), AUDIO_HAL_CODEC_MODE_DECODE, AUDIO_HAL_CTRL_START);
#endif

    ESP_LOGI(TAG, "Start handlers");

    ESP_LOGI(TAG, " - Start playback");
    pb_init(set);

    // xTaskCreate(print_all_tasks, "print_all_tasks", 4096, NULL, 5, NULL);

    if (rtc_storage.rtc_magic != 0xDEADC0DE || !rtc_checksum_valid())
    {
        ESP_LOGE(TAG, "RTC memory corrupted, reinit");
        memset(&rtc_storage, 0x00, sizeof(rtc_storage));
        rtc_storage.rtc_magic = 0xDEADC0DE;
        rtc_storage.volume = 50;
        rtc_storage.play_position = 0;
        rtc_checksum_update();
    }
    else if (rtc_storage.nfc_uid)
    {
        ESP_LOGI(TAG, "Inform playback handler UID %16llX / %lu", rtc_storage.nfc_uid, rtc_storage.play_position);
        pb_set_last(rtc_storage.nfc_uid, rtc_storage.play_position);
    }

    audio_hal_set_volume(audio_board_get_hal(), rtc_storage.volume);

    dac3100_set_mute(true);


    ESP_LOGI(TAG, " - Play startup tone");
    pb_play_default(CONTENT_DEFAULT_STARTUP);

    bool ear_big_prev = false;
    bool ear_small_prev = false;

    ESP_LOGI(TAG, " - Start Accel");
    accel_init(board_handle);
    ESP_LOGI(TAG, " - Start WiFi");
    wifi_init();
    ESP_LOGI(TAG, " - Start NFC");
    nfc_init();
    ESP_LOGI(TAG, " - Start Cloud");
    cloud_init();
    ESP_LOGI(TAG, " - Start OTA");
    ota_init();
    ESP_LOGI(TAG, " - Start WWW");
    www_init();

    //ESP_LOGI(TAG, "Unmount assets again");
    //esp_vfs_fat_spiflash_unmount("/spiflash", s_test_wl_handle);

    int64_t last_activity_time = esp_timer_get_time();
    int64_t last_adc_time = esp_timer_get_time();
    int64_t remute_time = 0;

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        int64_t cur_time = esp_timer_get_time();

        if (cur_time - last_adc_time > 1000000)
        {
            float vbatt = audio_board_get_vbatt();
            float vcell = vbatt / 3;

            const char *state = "";

            if(vcell < 0.9)
            {
                state = " <<LOW>>";
                ledman_set_system_state(SYSTEM_LOWBATT);
            }
            else if(vcell > 1.29)
            {
                state = " <<OVERVOLTAGE>>";
                ledman_set_system_state(SYSTEM_LOWBATT);
            }
            else if(vcell > 1.2)
            {
                state = " <<FULL>>";
            }
            ESP_LOGI(TAG, "Vbatt: %2.2f V (cell: %2.2f V)%s, Charger: %s", vbatt, vcell, state, (audio_board_get_vcharger() > 1.0) ? "CHARGING" : "idle");
            last_adc_time = cur_time;
        }

        bool ear_big = audio_board_ear_big();
        bool ear_small = audio_board_ear_small();

        if (ear_big && !ear_big_prev)
        {
            dac3100_set_mute(false);
            if (rtc_storage.volume < 100)
            {
                ESP_LOGI(TAG, "Volume up");
                rtc_storage.volume += 10;
                audio_hal_set_volume(audio_board_get_hal(), rtc_storage.volume);
                dac3100_beep(0, 0x140);
            }
            else
            {
                ESP_LOGI(TAG, "Volume up (limit)");
                dac3100_beep(1, 0x140);
            }
            remute_time = cur_time + 800;
        }

        if (ear_small && !ear_small_prev)
        {
            dac3100_set_mute(false);
            if (rtc_storage.volume >= 10)
            {
                ESP_LOGI(TAG, "Volume down");
                rtc_storage.volume -= 10;
                audio_hal_set_volume(audio_board_get_hal(), rtc_storage.volume);
                dac3100_beep(2, 0x140);
            }
            else
            {
                ESP_LOGI(TAG, "Volume down (limit)");
                dac3100_beep(3, 0x140);
            }
            remute_time = cur_time + 800;
        }

        if (remute_time != 0 && cur_time > remute_time)
        {
            remute_time = 0;
            dac3100_set_mute(!pb_is_playing());
        }

        if (ear_big || ear_small)
        {
            last_activity_time = cur_time;
        }
        
        if (pb_is_playing())
        {
            last_activity_time = cur_time;
            rtc_storage.nfc_uid = pb_get_current_uid();
            rtc_storage.play_position = pb_get_play_position();
        }

        if ((cur_time - last_activity_time) > POWEROFF_TIMEOUT)
        {
            break;
        }

        ear_big_prev = ear_big;
        ear_small_prev = ear_small;
    }

    ledman_change("poweroff");
    audio_board_sdcard_unmount();
    rtc_checksum_update();

    ESP_LOGI(TAG, "Poweroff");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    audio_board_poweroff();

    ESP_LOGE(TAG, "back, quite unexpected...");
}

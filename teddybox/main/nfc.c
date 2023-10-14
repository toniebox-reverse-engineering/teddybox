

#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "trf7962a.h"

#include "playback.h"
#include "board.h"
#include "nfc.h"

#define COUNT(x) (sizeof(x) / sizeof(x[0]))

static uint8_t slix_get_rand[] = {0x02, 0xB2, 0x04};
static uint8_t slix_get_inventory[] = {0x26, 0x01, 0x00};
static uint8_t slix_system_info[] = {0x22, 0x2b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static uint8_t slix_set_pass[] = {0x02, 0xB3, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00};

static uint8_t received_data[256];
static uint8_t received_length;

bool nfc_valid = false;
uint8_t nfc_current_uid[8];

static const char *TAG = "[NFC]";

typedef enum
{
    STATE_SEARCHING,
    STATE_TAG,
    STATE_SYSINFO
} nfc_state_t;

void nfc_dump(char *buffer, uint8_t *data, uint8_t length)
{
    int pos = 0;

    buffer[pos] = 0;

    for (int pos = 0; pos < length; pos++)
    {
        sprintf(&buffer[pos * 3], "%02X ", data[pos]);
    }
}

void nfc_log_dump(const char *title, uint8_t *data, uint8_t length)
{
    char dump_buf[129];
    nfc_dump(dump_buf, data, length);
    ESP_LOGD(TAG, "%s: %s", title, dump_buf);
}

esp_err_t nfc_get_rand(trf7962a_t trf, uint8_t *rand)
{
    if (trf7962a_xmit(trf, slix_get_rand, sizeof(slix_get_rand), received_data, &received_length) != ESP_OK)
    {
        return ESP_FAIL;
    }
    nfc_log_dump("GET RANDOM", received_data, received_length);
    if (received_length != 5 || received_data[0] != 0)
    {
        ESP_LOGE(TAG, "received RAND with %d bytes, status %d", received_length, received_data[0]);
        return ESP_FAIL;
    }

    rand[0] = received_data[1];
    rand[1] = received_data[2];

    return ESP_OK;
}

void nfc_reset(trf7962a_t trf)
{
    ESP_LOGD(TAG, "NFC Reset");
    trf7962a_field(trf, false);
    vTaskDelay(10 / portTICK_PERIOD_MS);
    trf7962a_reset(trf);
    trf7962a_field(trf, true);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

static void nfc_play()
{
    uint32_t uid = (nfc_current_uid[0] << 24) | (nfc_current_uid[1] << 16) | (nfc_current_uid[2] << 8) | nfc_current_uid[3];
    pb_play_content(uid);
}

static void nfc_stop()
{
    pb_stop();
}

void nfc_mainthread(void *arg)
{
    nfc_state_t state = STATE_SEARCHING;
    trf7962a_t trf = audio_board_get_trf();

    while (true)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        switch (state)
        {
        case STATE_SYSINFO:
        {
            if (trf7962a_xmit(trf, slix_system_info, sizeof(slix_system_info), received_data, &received_length) != ESP_OK)
            {
                nfc_reset(trf);
                ESP_LOGE(TAG, "Failed to read SYSINFO");
                state = STATE_SEARCHING;
                break;
            }
            nfc_log_dump("SYSINFO", received_data, received_length);
            break;
        }

        case STATE_TAG:
        {
            if (trf7962a_xmit(trf, slix_get_inventory, sizeof(slix_get_inventory), received_data, &received_length) != ESP_OK)
            {
                nfc_reset(trf);
                // ESP_LOGE(TAG, "Failed to read INVENTORY");
                state = STATE_SEARCHING;
                break;
            }
            nfc_log_dump("INVENTORY", received_data, received_length);
            if (received_length != 12 || received_data[0] != 0)
            {
                nfc_reset(trf);
                // ESP_LOGE(TAG, "received INVENTORY with %d bytes, status %d", received_length, received_data[0]);
                state = STATE_SEARCHING;
                break;
            }

            char dump_buf[129];
            nfc_dump(dump_buf, &received_data[2], 8);

            if (!nfc_valid)
            {
                nfc_valid = true;
                memcpy(nfc_current_uid, &received_data[2], 8);
                ESP_LOGI(TAG, "Tag entered: %s", dump_buf);
                nfc_play();
            }
            else
            {
                if (memcmp(nfc_current_uid, &received_data[2], 8))
                {
                    memcpy(nfc_current_uid, &received_data[2], 8);
                    ESP_LOGI(TAG, "Tag changed: %s", dump_buf);
                    nfc_play();
                }
            }
            memcpy(nfc_current_uid, &received_data[2], 8);
            vTaskDelay(250 / portTICK_PERIOD_MS);
            break;
        }

        case STATE_SEARCHING:
        {
            uint8_t rand[2];

            if (nfc_valid)
            {
                nfc_valid = false;
                char dump_buf[129];
                nfc_dump(dump_buf, nfc_current_uid, 8);
                ESP_LOGI(TAG, "Tag disappeared: %s", dump_buf);
                nfc_stop();
            }

            if (nfc_get_rand(trf, rand) != ESP_OK)
            {
                nfc_reset(trf);
                break;
            }
            if (trf7962a_xmit(trf, slix_get_inventory, sizeof(slix_get_inventory), received_data, &received_length) == ESP_OK)
            {
                ESP_LOGI(TAG, "Unlocked tag found");
                state = STATE_TAG;
                break;
            }
            ESP_LOGI(TAG, "Locked tag detected");

            bool unlocked = false;
            uint32_t passes[] = {0x0F0F0F0F, 0x7FFD6E5B, 0x00000000};

            for (int pass = 0; pass < COUNT(passes); pass++)
            {
                ESP_LOGI(TAG, "Test pass 0x%08X", passes[pass]);

                nfc_reset(trf);

                if (nfc_get_rand(trf, rand) != ESP_OK)
                {
                    ESP_LOGE(TAG, "GET RANDOM failed unexpectedly");
                    continue;
                }

                ESP_LOGD(TAG, "  RAND %02X %02X", rand[0], rand[1]);

                slix_set_pass[4] = (passes[pass] >> 0) ^ rand[0];
                slix_set_pass[5] = (passes[pass] >> 8) ^ rand[1];
                slix_set_pass[6] = (passes[pass] >> 16) ^ rand[0];
                slix_set_pass[7] = (passes[pass] >> 24) ^ rand[1];

                nfc_log_dump("SET PASS", slix_set_pass, sizeof(slix_set_pass));
                if (trf7962a_xmit(trf, slix_set_pass, sizeof(slix_set_pass), received_data, &received_length) != ESP_OK)
                {
                    nfc_reset(trf);
                    ESP_LOGE(TAG, "  Password incorrect");
                    continue;
                }
                nfc_log_dump("  SET PASS", received_data, received_length);
                if (received_length != 3 || received_data[0] != 0)
                {
                    nfc_reset(trf);
                    ESP_LOGE(TAG, "  Password incorrect - %d bytes, status %d", received_length, received_data[0]);
                    continue;
                }
                unlocked = true;
                break;
            }

            if (unlocked)
            {
                ESP_LOGI(TAG, "Unlocked tag");
                state = STATE_TAG;
            }

            break;
        }
        }
    }
}

void nfc_init()
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    xTaskCreatePinnedToCore(nfc_mainthread, "nfc_main", 8192, NULL, NFC_TASK_PRIO, NULL, tskNO_AFFINITY);
}

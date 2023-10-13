

#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "trf7962a.h"

#include "board.h"
#include "nfc.h"

static uint8_t slix_get_rand[] = {0x02, 0xB2, 0x04};
static uint8_t slix_get_inventory[] = {0x26, 0x01, 0x00};
static uint8_t slix_set_pass[] = {0x02, 0xB3, 0x04, 0x04, 0x00, 0x00, 0x00, 0x00};

uint8_t received_data[256];
uint8_t received_length;

static const char *TAG = "[NFC]";

#define COUNT(x) (sizeof(x) / sizeof(x[0]))

typedef enum
{
    STATE_IDLE,
    STATE_TAG
} nfc_state_t;

bool nfc_valid = false;
uint8_t nfc_current_uid[8];

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
    ESP_LOGI(TAG, "%s: %s", title, dump_buf);
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

void nfc_mainthread(void *arg)
{
    nfc_state_t state = STATE_IDLE;
    trf7962a_t trf = audio_board_get_trf();

    while (true)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        switch (state)
        {
        case STATE_TAG:
        {
            if (trf7962a_xmit(trf, slix_get_inventory, sizeof(slix_get_inventory), received_data, &received_length) != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to read INVENTORY");
                state = STATE_IDLE;
                break;
            }
            nfc_log_dump("INVENTORY", received_data, received_length);
            if (received_length != 14 || received_data[0] != 0)
            {
                ESP_LOGE(TAG, "received INVENTORY with %d bytes, status %d", received_length, received_data[0]);
                state = STATE_IDLE;
                break;
            }
            memcpy(nfc_current_uid, &received_data[2], 8);
            vTaskDelay(250 / portTICK_PERIOD_MS);
            break;
        }

        case STATE_IDLE:
        {
            uint8_t rand[2];
            nfc_valid = false;

            if (nfc_get_rand(trf, rand) != ESP_OK)
            {
                break;
            }
            ESP_LOGI(TAG, "Tag detected");

            bool unlocked = false;
            uint32_t passes[] = {0x7FFD6E5B, 0x0F0F0F0F, 0x00000000};

            for (int pass = 0; pass < COUNT(passes); pass++)
            {
                ESP_LOGI(TAG, "Test pass 0x%08X", passes[pass]);

                if (nfc_get_rand(trf, rand) != ESP_OK)
                {
                    ESP_LOGE(TAG, "GET RANDOM failed unexpectedly");
                    continue;
                }

                ESP_LOGI(TAG, "  RAND %02X %02X", rand[0], rand[1]);

                slix_set_pass[4] = (passes[pass] >> 0) ^ rand[0];
                slix_set_pass[5] = (passes[pass] >> 8) ^ rand[1];
                slix_set_pass[6] = (passes[pass] >> 16) ^ rand[0];
                slix_set_pass[7] = (passes[pass] >> 24) ^ rand[1];

                if (trf7962a_xmit(trf, slix_set_pass, sizeof(slix_set_pass), received_data, &received_length) != ESP_OK)
                {
                    ESP_LOGE(TAG, "  Password incorrect");
                    trf7962a_field(trf, false);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    trf7962a_field(trf, true);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    continue;
                }
                nfc_log_dump("  SET PASS", received_data, received_length);
                if (received_length != 3 || received_data[0] != 0)
                {
                    ESP_LOGE(TAG, "  Password incorrect - %d bytes, status %d", received_length, received_data[0]);
                    continue;
                }
                unlocked = true;
                break;
            }

            if (unlocked)
            {
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

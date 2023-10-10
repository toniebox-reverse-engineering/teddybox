

#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "accel.h"
#include "board.h"

static const char *TAG = "[ACC]";

void accel_mainthread(void *arg)
{
    audio_board_handle_t board = (audio_board_handle_t)arg;
    float accel[3];

    board->lis3dh->set_data_rate(board->lis3dh, 20);
    while (1)
    {
        vTaskDelay(50 / portTICK_RATE_MS);
        board->lis3dh->fetch(board->lis3dh, accel);
        ESP_LOGD(TAG, "Accel: X %2.2f, Y %2.2f, Z %2.2f", accel[0], accel[1], accel[2]);
    }
}

void accel_init(audio_board_handle_t board)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    xTaskCreatePinnedToCore(accel_mainthread, "accel_main", 4096, (void *)board, ACCEL_TASK_PRIO, NULL, tskNO_AFFINITY);
}

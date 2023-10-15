

#include <stdbool.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "esp_log.h"

#include "accel.h"
#include "board.h"
#include "playback.h"

static const char *TAG = "[ACC]";

static float getRoll(float ax, float ay, float az)
{
    return atan2(ay, az) * 180.0 / M_PI;
}

static float getPitch(float ax, float ay, float az)
{
    return atan2(-ax, sqrt(ay * ay + az * az)) * 180.0 / M_PI;
}

typedef enum
{
    STATE_UNKNOWN,
    STATE_STABLE_MAYBE,
    STATE_STABLE,
    STATE_TILTED_MAYBE,
    STATE_TILTED,
    STATE_FLIPPED_MAYBE,
    STATE_FLIPPED
} accel_state_t;

static accel_state_t current_state = STATE_UNKNOWN;
uint32_t state_counter = 0;

bool accel_within(float val, float target, float limit)
{
    float difference = fabs(fabs(val) - target);

    if (difference < limit)
    {
        return true;
    }

    if (difference > 360.0 - limit)
    {
        return true;
    }

    return false;
}

void accel_handle_angle(float roll, float pitch)
{
    static float target_roll = 0;
    static float target_pitch = 0;
    static float target_limit = 0;
    static bool was_stable = false;

    if (!accel_within(roll, target_roll, target_limit) || !accel_within(pitch, target_pitch, target_limit))
    {
        current_state = STATE_UNKNOWN;
        state_counter = 0;
    }

    switch (current_state)
    {
    case STATE_UNKNOWN:
    {
        if (accel_within(roll, ACCEL_ANGLE_STABLE, ACCEL_ANGLE_TILT / 2) && accel_within(pitch, ACCEL_ANGLE_STABLE, ACCEL_ANGLE_TILT / 2))
        {
            target_roll = 0;
            target_pitch = 0;
            target_limit = ACCEL_ANGLE_TILT / 2;
            current_state = STATE_STABLE_MAYBE;
            ESP_LOGI(TAG, "STATE_STABLE_MAYBE");
        }
        if (accel_within(roll, ACCEL_ANGLE_TILT, ACCEL_ANGLE_TILT / 2) && accel_within(pitch, ACCEL_ANGLE_TILT, ACCEL_ANGLE_TILT / 2))
        {
            target_roll = ACCEL_ANGLE_TILT;
            target_pitch = ACCEL_ANGLE_TILT;
            target_limit = ACCEL_ANGLE_TILT / 2;
            current_state = STATE_TILTED_MAYBE;
            ESP_LOGI(TAG, "STATE_TILTED_MAYBE");
        }
        if (accel_within(roll, ACCEL_ANGLE_FLIP, ACCEL_ANGLE_TILT / 2) && accel_within(pitch, ACCEL_ANGLE_STABLE, ACCEL_ANGLE_TILT / 2))
        {
            target_roll = ACCEL_ANGLE_FLIP;
            target_pitch = ACCEL_ANGLE_STABLE;
            target_limit = ACCEL_ANGLE_TILT / 2;
            current_state = STATE_FLIPPED_MAYBE;
            ESP_LOGI(TAG, "STATE_FLIPPED_MAYBE");
        }
        break;
    }

    case STATE_STABLE_MAYBE:
    case STATE_TILTED_MAYBE:
    case STATE_FLIPPED_MAYBE:
        if (state_counter++ >= ACCEL_STABLE_DELAY)
        {
            ESP_LOGI(TAG, "Considering angle stable");
            current_state++;
            state_counter = 0;
        }
        break;

    case STATE_STABLE:
        was_stable = true;
        break;

    case STATE_TILTED:
        if (roll > 0 && pitch > 0)
        {
            if (state_counter++ == 0 && was_stable)
            {
                was_stable = false;
                ESP_LOGI(TAG, "emit CHAPTER -");
                pb_seek_chapter(-1);
            }
        }
        if (roll < 0 && pitch < 0)
        {
            if (state_counter++ == 0 && was_stable)
            {
                was_stable = false;
                ESP_LOGI(TAG, "emit CHAPTER +");
                pb_seek_chapter(1);
            }
        }
        if (roll < 0 && pitch > 0)
        {
            if (state_counter++ == 0)
            {
                was_stable = false;
                ESP_LOGI(TAG, "emit SEEK +");
                pb_seek(ACCEL_SEEK_BLOCKS);
            }
            /* repeat action */
            if (state_counter > ACCEL_SEEK_REPEAT)
            {
                state_counter = 0;
            }
        }
        if (roll > 0 && pitch < 0)
        {
            if (state_counter++ == 0)
            {
                was_stable = false;
                ESP_LOGI(TAG, "emit SEEK -");
                pb_seek(-ACCEL_SEEK_BLOCKS);
            }
            /* repeat action */
            if (state_counter > ACCEL_SEEK_REPEAT)
            {
                state_counter = 0;
            }
        }
        break;

    case STATE_FLIPPED:
        ESP_LOGI(TAG, "emit FLIPPED");
        break;
    }
}

void accel_handle(float x, float y, float z)
{
    /* nothing implemented yet. probably needs higher update rate or interrupts */
}

void accel_mainthread(void *arg)
{
    audio_board_handle_t board = (audio_board_handle_t)arg;
    float accel[3];

    board->lis3dh->set_data_rate(board->lis3dh, ACCEL_DATA_RATE);
    while (1)
    {
        /* polling is not the best way. better study datasheet and use the sensor's internal features */
        vTaskDelay(ACCEL_LOOP_MS / portTICK_RATE_MS);

        esp_err_t err = board->lis3dh->fetch(board->lis3dh, accel);
        if(err != ESP_OK)
        {
            continue;
        }
        float x = accel[2];
        float y = accel[1];
        float z = -accel[0];

        float roll = getRoll(x, y, z);
        float pitch = getPitch(x, y, z);

        // ESP_LOGI(TAG, "Accel: X %2.2f, Y %2.2f, Z %2.2f, R %2.2f, P %2.2f", x, y, z, roll, pitch);

        accel_handle(x, y, z);
        accel_handle_angle(roll, pitch);
    }
}

void accel_init(audio_board_handle_t board)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    xTaskCreatePinnedToCore(accel_mainthread, "accel_main", 2048, (void *)board, ACCEL_TASK_PRIO, NULL, tskNO_AFFINITY);
}

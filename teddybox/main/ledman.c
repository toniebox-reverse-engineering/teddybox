#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include <string.h>

#include "led.h"
#include "ledman.h"

static const char *TAG = "LEDMan";
static QueueHandle_t ledman_queue;

const State states[] = {
    {.name = "fade in",
     .seq = (const SequenceCommand[]){
         SET_COLOR(100, 100, 100),
         FADE(0, 0, 100, 300, 10),
         FADE(100, 0, 100, 300, 10),
         FADE(0, 100, 0, 800, 10),
         END()}},

    {.name = "on", .seq = (const SequenceCommand[]){SET_COLOR(0, 100, 0), END()}},
    {.name = "off", .seq = (const SequenceCommand[]){SET_COLOR(0, 0, 0), END()}},
    {.name = "fade off", .seq = (const SequenceCommand[]){FADE(0, 0, 0, 500, 10), END()}},

    {.name = "red", .seq = (const SequenceCommand[]){SET_COLOR(100, 0, 0), END()}},
    {.name = "green", .seq = (const SequenceCommand[]){SET_COLOR(0, 100, 0), END()}},
    {.name = "blue", .seq = (const SequenceCommand[]){SET_COLOR(0, 0, 100), END()}},
    {.name = "white", .seq = (const SequenceCommand[]){SET_COLOR(100, 100, 100), END()}},

    {.name = "fade green", .seq = (const SequenceCommand[]){FADE(0, 100, 0, 200, 10), END()}},

    {.name = "rose", .seq = (const SequenceCommand[]){SET_COLOR(100, 40, 40), END()}},

    {.name = "fadeblink red", .seq = (const SequenceCommand[]){SET_COLOR(100, 0, 0), DELAY(500), SET_COLOR(0, 0, 0), DELAY(500), LOOP()}},
    {.name = "fadeblink green", .seq = (const SequenceCommand[]){SET_COLOR(0, 100, 0), DELAY(500), SET_COLOR(0, 0, 0), DELAY(500), LOOP()}},
    {.name = "fadeblink blue", .seq = (const SequenceCommand[]){SET_COLOR(0, 0, 100), DELAY(500), SET_COLOR(0, 0, 0), DELAY(500), LOOP()}},

    {.name = "fadeblink blue-green", .seq = (const SequenceCommand[]){FADE(0, 0, 100, 200, 10), FADE(0, 100, 0, 200, 10), LOOP()}},
    {.name = "fadeblink blue-red", .seq = (const SequenceCommand[]){FADE(0, 0, 100, 200, 10), FADE(100, 0, 0, 200, 10), LOOP()}},

    {.name = "fadeblink blue-red slow", .seq = (const SequenceCommand[]){FADE(0, 0, 100, 800, 10), FADE(100, 0, 0, 800, 10), LOOP()}},
    {.name = "fadeblink blue-green slow", .seq = (const SequenceCommand[]){FADE(0, 0, 100, 1000, 10), FADE(0, 100, 0, 1000, 10), LOOP()}}
};

typedef enum
{
    SYSTEM_NORMAL,
    SYSTEM_OFFLINE,
    SYSTEM_LOWBATT,
    NUM_SYSTEM_STATES // Keep this last
} SystemState;

typedef struct
{
    const char *requestedState;
    const char *actualState;
} StateMapping;

static const StateMapping *stateMappings[NUM_SYSTEM_STATES] = {
    [SYSTEM_NORMAL] = (const StateMapping[]){
        {"off", "off"},
        {"poweroff", "fade off"},
        {"idle", "fade green"},
        {"checking", "fadeblink blue-green"},
        {"playing", "green"},
        {"playing download", "fadeblink blue-green slow"},
        {"failed", "blink red"},
        {NULL, NULL}},
    [SYSTEM_OFFLINE] = (const StateMapping[]) {
        {"off", "off"},
        {"poweroff", "fade off"},
        {"idle", "white"},
        {"checking", "fadeblink blue-green"},
        {"playing", "green"},
        {"playing download", "fadeblink blue-green slow"},
        {"failed", "blink red"},
        {NULL, NULL}},
    [SYSTEM_LOWBATT] = (const StateMapping[]){
        {"off", "off"},
        {"poweroff", "fade off"},
        {"idle", "rose"},
        {"checking", "fadeblink blue-red"},
        {"playing", "fadeblink green-red"},
        {"playing download", "fadeblink blue-red slow"},
        {"failed", "blink red"},
        {NULL, NULL}}};

static SystemState current_system_state = SYSTEM_NORMAL;

static bool ledman_sleep(uint32_t delay)
{
    const char *peek_state;
    if (xQueuePeek(ledman_queue, &peek_state, delay / portTICK_PERIOD_MS))
    {
        return true;
    }

    return false;
}

static void ledman_execute(const SequenceCommand *sequence)
{
    static float current_r = 0;
    static float current_g = 0;
    static float current_b = 0;

    int pos = 0;
    while (1)
    {
        SequenceCommand cmd = sequence[pos];
        pos++;

        switch (cmd.type)
        {
        case COMMAND_LOOP:
        {
            pos = 0;
            continue;
        }

        case COMMAND_END:
        {
            return;
        }

        case COMMAND_SET_COLOR:
        {
            current_r = cmd.color.r;
            current_g = cmd.color.g;
            current_b = cmd.color.b;
            led_set_rgb(current_r, current_g, current_b);
            break;
        }

        case COMMAND_DELAY:
        {
            if (ledman_sleep(cmd.delay.ms))
            {
                return;
            }
            break;
        }

        case COMMAND_FADE:
        {
            float target_r = cmd.fade.r;
            float target_g = cmd.fade.g;
            float target_b = cmd.fade.b;
            uint32_t duration = cmd.fade.duration;
            uint32_t step_duration = cmd.fade.step;
            uint32_t steps_per_transition = duration / step_duration;

            for (uint32_t i = 0; i <= steps_per_transition; i++)
            {
                float r = current_r + (target_r - current_r) * i / steps_per_transition;
                float g = current_g + (target_g - current_g) * i / steps_per_transition;
                float b = current_b + (target_b - current_b) * i / steps_per_transition;
                led_set_rgb(r, g, b);
                if (ledman_sleep(step_duration))
                {
                    return;
                }
            }

            current_r = target_r;
            current_g = target_g;
            current_b = target_b;
            break;
        }
        }
    }
}

void ledman_set_system_state(SystemState state)
{
    current_system_state = state;
}

void ledman_change(const char *requestedState)
{
    const char *actualState = requestedState;
    const StateMapping *currentMappings = stateMappings[current_system_state];

    for (int i = 0; currentMappings[i].requestedState != NULL; ++i)
    {
        if (strcmp(requestedState, currentMappings[i].requestedState) == 0)
        {
            actualState = currentMappings[i].actualState;
            break;
        }
    }

    xQueueSend(ledman_queue, &actualState, portMAX_DELAY);
}

void ledman_task(void *arg)
{
    const char *current_state;

    while (1)
    {
        if (xQueueReceive(ledman_queue, &current_state, portMAX_DELAY))
        {
            for (size_t i = 0; i < sizeof(states) / sizeof(State); ++i)
            {
                if (strcmp(current_state, states[i].name) == 0)
                {
                    ledman_execute(states[i].seq);
                    break;
                }
            }
        }
    }
}

void ledman_init()
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    ledman_queue = xQueueCreate(10, sizeof(char *));
    xTaskCreate(ledman_task, "ledman_task", 2048, NULL, 5, NULL);

    ledman_change("fade in");
}

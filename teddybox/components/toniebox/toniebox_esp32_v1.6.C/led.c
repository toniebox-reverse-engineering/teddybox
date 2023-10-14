
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm.h"

#include "board.h"

static const char *TAG = "LED";
void led_startup_task(void *ctx)
{
    float duration = 1.0;  // Total duration in seconds
    float step_duration = 0.03;  // Step duration in seconds
    int steps_per_transition;  // Number of steps for each transition (white-blue, blue-purple, purple-green)
    int r, g, b;
    int i;

    steps_per_transition = (int)(duration / (3 * step_duration));  // Calculate steps for each transition
    step_duration *= 1000;  // Convert to milliseconds

    // White to Blue
    for (i = 0; i <= steps_per_transition; i++)
    {
        r = 100 - (i * 100 / steps_per_transition);
        g = 100 - (i * 100 / steps_per_transition);
        b = 100;
        led_set_rgb(r, g, b);
        vTaskDelay(step_duration / portTICK_PERIOD_MS);
    }

    // Blue to Purple
    for (i = 0; i <= steps_per_transition; i++)
    {
        r = i * 100 / steps_per_transition;
        g = 0;
        b = 100;
        led_set_rgb(r, g, b);
        vTaskDelay(step_duration / portTICK_PERIOD_MS);
    }

    // Purple to Green
    for (i = 0; i <= steps_per_transition; i++)
    {
        r = 100 - (i * 100 / steps_per_transition);
        g = i * 100 / steps_per_transition;
        b = 100 - (i * 100 / steps_per_transition);
        led_set_rgb(r, g, b);
        vTaskDelay(step_duration / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}


void led_init(void)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);
    
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, LED_RED_GPIO);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM1A, LED_GREEN_GPIO);
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM2A, LED_BLUE_GPIO);

    mcpwm_config_t pwm_config = {
        .frequency = 1000,
        .cmpr_a = 0,
        .counter_mode = MCPWM_UP_COUNTER,
        .duty_mode = MCPWM_DUTY_MODE_0,
    };
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_1, &pwm_config);
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_2, &pwm_config);
    
    xTaskCreate(led_startup_task, "led_startup_task", 2048, NULL, 5, NULL);
}

void led_set(led_t led, float pct)
{
    mcpwm_timer_t timer = MCPWM_TIMER_0;

    switch(led)
    {
        case LED_RED:
        timer = MCPWM_TIMER_0;
        break;

        case LED_GREEN:
        timer = MCPWM_TIMER_1;
        break;

        case LED_BLUE:
        timer = MCPWM_TIMER_2;
        break;
    }
    mcpwm_set_duty(MCPWM_UNIT_0, timer, MCPWM_GEN_A, pct);
}

void led_set_rgb(float r, float g, float b)
{
    led_set(LED_RED, r);
    led_set(LED_GREEN, g);
    led_set(LED_BLUE, b);
}

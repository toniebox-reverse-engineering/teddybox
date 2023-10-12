
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/mcpwm.h"

#include "board.h"

static const char *TAG = "LED";

void led_init(void)
{
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

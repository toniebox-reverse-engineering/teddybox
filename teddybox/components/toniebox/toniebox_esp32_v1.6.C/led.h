#pragma once

typedef enum
{
    LED_RED,
    LED_GREEN,
    LED_BLUE,
} led_t;

void led_init(void);
void led_set(led_t led, float pct);
void led_set_rgb(float r, float g, float b);

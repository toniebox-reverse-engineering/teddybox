
#pragma once

#include "esp_peripherals.h"

#define PB_TASK_PRIO 10
#define PB_QUEUE_SIZE 10


void pb_init(esp_periph_set_handle_t set);
void pb_mainthread(void *arg);
void pb_play(const char *uri);
void pb_deinit(void);

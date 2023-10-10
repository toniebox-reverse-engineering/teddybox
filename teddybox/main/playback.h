
#pragma once

#include "esp_peripherals.h"

#define PB_TASK_PRIO 10
#define PB_QUEUE_SIZE 10


#define CONTENT_DEFAULT_START   0x00000000
#define CONTENT_DEFAULT_LOWBATT 0x00000003
#define CONTENT_DEFAULT_DL_DONE 0x00000006


void pb_init(esp_periph_set_handle_t set);
void pb_mainthread(void *arg);
void pb_deinit(void);

esp_err_t pb_play(const char *uri);
esp_err_t pb_play_default_lang(uint32_t lang, uint32_t id);
esp_err_t pb_play_default(uint32_t id);
esp_err_t pb_play_content(uint32_t id);

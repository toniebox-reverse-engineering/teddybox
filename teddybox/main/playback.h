
#pragma once

#include "esp_peripherals.h"

#define PB_TASK_PRIO 10
#define PB_QUEUE_SIZE 10

/* minimum number of blocks to download before playback starts */
#define PB_MIN_DL_BLOCKS 20

#define CONTENT_DEFAULT_STARTUP 0x00000000
#define CONTENT_DEFAULT_TADA 0x00000001
#define CONTENT_DEFAULT_TADUM 0x00000002
#define CONTENT_DEFAULT_LOW_BATTERY_WARN 0x00000003
#define CONTENT_DEFAULT_EMPTY1 0x00000004
#define CONTENT_DEFAULT_EMPTY2 0x00000005
#define CONTENT_DEFAULT_DOWNLOAD_DONE 0x00000006
#define CONTENT_DEFAULT_OFFLINE_ON 0x00000007
#define CONTENT_DEFAULT_OFFLINE_OFF 0x00000008
#define CONTENT_DEFAULT_LOW_BATTERY_SHUTDOWN 0x00000009
#define CONTENT_DEFAULT_CODE_KOALA 0x0000000A
#define CONTENT_DEFAULT_INSTALLATION 0x0000000B
#define CONTENT_DEFAULT_EMPTY3 0x0000000C
#define CONTENT_DEFAULT_ON_CHARGER 0x0000000D
#define CONTENT_DEFAULT_LIMIT_REACHED 0x0000000E
#define CONTENT_DEFAULT_NETWORK_ERROR 0x0000000F
#define CONTENT_DEFAULT_BOX_READY 0x00000010
#define CONTENT_DEFAULT_CODE_TURTLE 0x00000011
#define CONTENT_DEFAULT_CODE_MARMOT 0x00000012
#define CONTENT_DEFAULT_WRONG_PASSWORD 0x00000013
#define CONTENT_DEFAULT_CODE_HEDGEHOG 0x00000014
#define CONTENT_DEFAULT_CODE_ANT 0x00000015
#define CONTENT_DEFAULT_CODE_MEERKAT 0x00000016
#define CONTENT_DEFAULT_CODE_OWL 0x00000017
#define CONTENT_DEFAULT_CODE_ELEPHANT 0x00000018

#define TONIEFILE_FRAME_SIZE 4096
#define TONIEFILE_MAX_CHAPTERS 100
#define TONIEFILE_PAD_END 64

void pb_init(esp_periph_set_handle_t set);
void pb_mainthread(void *arg);
void pb_deinit(void);
esp_err_t pb_seek(int32_t blocks);
esp_err_t pb_seek_chapter(int32_t chapters);
esp_err_t pb_set_chapter(int32_t chapter);
int32_t pb_get_chapter(void);

esp_err_t pb_play(const char *uri);
esp_err_t pb_play_default_lang(uint32_t lang, uint32_t id);
esp_err_t pb_play_default(uint32_t id);
esp_err_t pb_play_content(uint64_t nfc_uid);
esp_err_t pb_play_content_token(uint64_t nfc_uid, const uint8_t *token);
esp_err_t pb_stop();
char *pb_build_filename(uint64_t id);

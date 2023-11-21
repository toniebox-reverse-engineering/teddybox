

#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"

#include "sdkconfig.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "opus_decoder.h"
#include "i2s_stream.h"
#include "raw_stream.h"

#include "playback.h"
#include "dac3100.h"
#include "board.h"
#include "math.h"
#include "ledman.h"
#include "cloud.h"

#include "config.h"


audio_pipeline_handle_t pipeline;
audio_element_handle_t i2s_stream_writer, music_decoder;
audio_event_iface_handle_t evt;

static QueueHandle_t playback_queue;
static bool pb_default_content = false;
static bool pb_playing = false;
static pb_toniefile_t pb_toniefile_info;
static cloud_content_req_t *current_dl_req = NULL;

static uint64_t pb_last_nfc_uid = 0;
static uint32_t pb_last_play_position = 0;

static const char *TAG = "[PB]";

/* ********************************************************************************************************************* */
/* ********************************************************************************************************************* */
/* ********************************************************************************************************************* */

TonieboxAudioFileHeader *pb_toniefile_get_header(FILE *fd)
{
    fseek(fd, 0, SEEK_SET);

    uint8_t proto_be[4];
    if (fread(proto_be, 4, 1, fd) != 1)
    {
        ESP_LOGE(TAG, "Failed to read header size");
        return NULL;
    }
    uint32_t proto_size = (proto_be[0] << 24) | (proto_be[1] << 16) | (proto_be[2] << 8) | proto_be[3];
    if (proto_size > TONIEFILE_FRAME_SIZE)
    {
        ESP_LOGE(TAG, "Invalid header size: 0x%04lX", proto_size);
        return NULL;
    }
    uint8_t *buffer = malloc(proto_size);
    if (fread(buffer, proto_size, 1, fd) != 1)
    {
        free(buffer);
        ESP_LOGE(TAG, "Failed to read header");
        return NULL;
    }

    TonieboxAudioFileHeader *taf = toniebox_audio_file_header__unpack(NULL, proto_size, (const uint8_t *)buffer);
    free(buffer);
    if (!taf)
    {
        ESP_LOGE(TAG, "Failed to parse header");
        return NULL;
    }

    return taf;
}

esp_err_t pb_toniefile_open(pb_toniefile_t *info, const char *filepath)
{
    memset(info, 0x00, sizeof(pb_toniefile_t));

    info->filename = strdup(filepath);

    if (current_dl_req)
    {
        ESP_LOGI(TAG, "Download in progress, using already open file");
        while (current_dl_req->state < CC_STATE_RECEIVING)
        {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        while (!xSemaphoreTake(current_dl_req->file_sem, 1000 / portTICK_PERIOD_MS))
        {
            ESP_LOGE(TAG, "Open: Timed out waiting for file lock...");
        }
        info->taf = pb_toniefile_get_header(current_dl_req->handle);
        xSemaphoreGive(current_dl_req->file_sem);
        if (!info->taf)
        {
            free(info->filename);
            return ESP_FAIL;
        }
    }
    else
    {
        ESP_LOGI(TAG, "Open Toniefile: %s", info->filename);
#ifndef DEVBOARD
        info->fd = fopen(info->filename, "rb");
        if (!info->fd)
        {
            ESP_LOGE(TAG, "Failed to read file: %s", info->filename);
            free(info->filename);
            return ESP_FAIL;
        }
        info->taf = pb_toniefile_get_header(info->fd);
        if (!info->taf)
        {
            fclose(info->fd);
            free(info->filename);
            return ESP_FAIL;
        }
#else
        extern const unsigned char dummy_audio_start[] asm("_binary_00000000_start");
        extern const unsigned char dummy_audio_end[] asm("_binary_00000000_end");
        const size_t dummy_audio_size = (dummy_audio_end - dummy_audio_start);

        info->taf = malloc(sizeof(TonieboxAudioFileHeader));
        info->taf->audio_id = 0xDEADBEEF;
        info->taf->num_bytes = dummy_audio_size;
        info->taf->n_track_page_nums = 1;
        info->taf->track_page_nums = malloc(sizeof(uint32_t));
        info->taf->track_page_nums[0] = 0;
#endif
    }

    ESP_LOGI(TAG, "  Audio ID: %08lX", info->taf->audio_id);
    ESP_LOGI(TAG, "  Size:     %08llX", info->taf->num_bytes);
    ESP_LOGI(TAG, "  Chapters: %d", info->taf->n_track_page_nums);
    for (int chap = 0; chap < info->taf->n_track_page_nums; chap++)
    {
        ESP_LOGI(TAG, "    %d: offset %08lX", chap, info->taf->track_page_nums[chap]);
    }

    info->current_pos = TONIEFILE_FRAME_SIZE;
    info->current_block = 0;
    info->current_chapter = 0;
    info->target_chapter = -1;
    info->target_pos = -1;
    info->seek_blocks = 0;
    info->valid = true;
    info->initialized = false;

    return ESP_OK;
}

void pb_toniefile_close(pb_toniefile_t *info)
{
    if (!info->valid)
    {
        return;
    }

    FILE *fd = info->fd;
    info->valid = false;
    info->fd = NULL;
    /* ToDo: all of that remote/local playback thing has to be coordinated. for now just leave handles open */
    if (!current_dl_req && fd)
    {
        fclose(fd);
    }
    if(info->taf)
    {
        toniebox_audio_file_header__free_unpacked(info->taf, NULL);
    }
    info->taf = NULL;
    free(info->filename);
    info->filename = NULL;
}

static size_t pb_ensure_block(pb_toniefile_t *info, FILE *fd)
{
#ifdef DEVBOARD
    extern const unsigned char dummy_audio_start[] asm("_binary_00000000_start");
    extern const unsigned char dummy_audio_end[] asm("_binary_00000000_end");
    const size_t dummy_audio_size = (dummy_audio_end - dummy_audio_start);
#endif

    bool read = false;

    /* when starting with no block preselected, we can safely start playing at the first block */
    if (info->current_block == 0)
    {
        info->initialized = true;
    }

        ESP_LOGI(TAG, "ensure_block pos: %ld", info->current_pos);
    /* else, before we can play any position, read the initial ogg header block */
    if (!info->initialized)
    {
        if (info->current_pos == TONIEFILE_FRAME_SIZE)
        {
#ifndef DEVBOARD
            fseek(fd, TONIEFILE_FRAME_SIZE, SEEK_SET);
            info->current_block_avail = fread(info->current_block_buffer, 1, 0x200, fd);
#else
            memcpy(info->current_block_buffer, &dummy_audio_start[TONIEFILE_FRAME_SIZE], 0x200);
            info->current_block_avail = 0x200;
#endif
        }

        /* before we continue, first consume the initial Ogg block */
        size_t remain = info->current_block_avail - (info->current_pos % TONIEFILE_FRAME_SIZE);
        if (remain)
        {
            return remain;
        }

        /* consumed the 0x200 bytes "first-block", proceed to normal operation */
        info->current_pos = info->current_block * TONIEFILE_FRAME_SIZE;
        info->current_block = 0;
        info->initialized = true;
    }

    if (info->current_pos >= (info->current_block + 1) * TONIEFILE_FRAME_SIZE)
    {
        read = true;
    }
    if (info->current_pos < info->current_block * TONIEFILE_FRAME_SIZE)
    {
        read = true;
    }

    if (read)
    {
        info->current_block = info->current_pos / TONIEFILE_FRAME_SIZE;
        
#ifndef DEVBOARD
        fseek(fd, info->current_block * TONIEFILE_FRAME_SIZE, SEEK_SET);
        info->current_block_avail = fread(info->current_block_buffer, 1, TONIEFILE_FRAME_SIZE, fd);
        if (info->current_block_avail < 0)
        {
            info->current_block_avail = 0;
        }
#else
        uint32_t pos = info->current_block * TONIEFILE_FRAME_SIZE;
        info->current_block_avail = TONIEFILE_FRAME_SIZE;
        if(pos + info->current_block_avail > dummy_audio_size)
        {
            info->current_block_avail = dummy_audio_size - pos;
        }

        memcpy(info->current_block_buffer, &dummy_audio_start[pos], info->current_block_avail);
#endif
    }
    if (!pb_default_content)
    {
        pb_last_play_position = info->current_block;
    }

    size_t remain = info->current_block_avail - (info->current_pos % TONIEFILE_FRAME_SIZE);

    return remain;
}

int pb_toniefile_cbr(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    pb_toniefile_t *info = (pb_toniefile_t *)context;

    if (!info || !info->valid)
    {
        ESP_LOGE(TAG, "Playback already finished, but was called again");
        return AEL_IO_DONE;
    }

    if(xPortInIsrContext())
    {
        ESP_LOGE(TAG, "Called from interrupt");
        return AEL_IO_DONE;
    }
        ESP_LOGI(TAG, "opus_cbr len: %d", len);


    /* doing seeking here, saves us some semaphores*/
    if (info->target_chapter >= 0)
    {
        ESP_LOGI(TAG, "Set target chapter %ld", info->target_chapter);
        if (info->target_chapter < info->taf->n_track_page_nums)
        {
            uint32_t block = 1 + info->taf->track_page_nums[info->target_chapter];
            uint32_t offset = block * TONIEFILE_FRAME_SIZE;

            ESP_LOGI(TAG, " -> block %ld, offset %ld", block, offset);
            info->target_pos = offset;
        }
        info->target_chapter = -1;
    }

    if (info->seek_blocks)
    {
        int32_t skip_bytes = info->seek_blocks * TONIEFILE_FRAME_SIZE;

        if (info->current_pos + skip_bytes < 0)
        {
            info->target_pos = 0;
        }
        else
        {
            info->target_pos = info->current_pos + skip_bytes;
        }
        ESP_LOGI(TAG, "Seek %ld blocks, current offset 0x%08lX, target 0x%08lX", info->seek_blocks, info->current_pos, info->target_pos);
        info->seek_blocks = 0;
    }

    /* operations above want us to seek to a certain file offset. do that here */
    if (info->target_pos >= 0)
    {
        /* do not seek to the stream start, else the decoder sees a stream start packet which causes errors */
        if (info->target_pos < 2 * TONIEFILE_FRAME_SIZE)
        {
            info->target_pos = 2 * TONIEFILE_FRAME_SIZE;
        }

        /* before seeking, wait until we are block-aligned again */
        if ((info->current_pos % TONIEFILE_FRAME_SIZE) == 0)
        {
            ESP_LOGI(TAG, "Seeking possible");
            info->current_pos = info->target_pos;
            info->target_pos = -1;
        }
        else
        {
            /* not yet, only read up to block end */
            uint32_t max_len = TONIEFILE_FRAME_SIZE - (info->current_pos % TONIEFILE_FRAME_SIZE);

            if (len > max_len)
            {
                ESP_LOGI(TAG, "Seeking delayed, only reading %d bytes to match block size", len);
                len = max_len;
            }
        }
    }

    /* update chapter info */
    {
        int32_t current_block = (info->current_pos / TONIEFILE_FRAME_SIZE) - 1;

        for (int chap = 0; chap < info->taf->n_track_page_nums; chap++)
        {
            if (current_block < info->taf->track_page_nums[chap])
            {
                if (info->current_chapter != chap - 1)
                {
                    info->current_chapter = chap - 1;
                    ESP_LOGI(TAG, "Current chapter: %ld", info->current_chapter);
                }
                break;
            }
        }
    }

    int bytes_read = 0;
    int avail = 0;
    if (current_dl_req)
    {
        while (!xSemaphoreTake(current_dl_req->file_sem, 1000 / portTICK_PERIOD_MS))
        {
            ESP_LOGE(TAG, "Playback: Timed out waiting for file lock...");
        }
        avail = pb_ensure_block(info, current_dl_req->handle);
        xSemaphoreGive(current_dl_req->file_sem);
    }
    else
    {
        avail = pb_ensure_block(info, info->fd);
    }

    if (len > avail)
    {
        len = avail;
    }
    int offset = info->current_pos % TONIEFILE_FRAME_SIZE;
    memcpy(buffer, &info->current_block_buffer[offset], len);
    bytes_read = len;

    if (bytes_read > 0)
    {
        info->current_pos += bytes_read;
    }
    else if (bytes_read == 0)
    {
        ESP_LOGI(TAG, "Playback finished, cleaning up");
        pb_toniefile_close(info);
        return AEL_IO_DONE;
    }
    else
    {
        ESP_LOGI(TAG, "Reading failed, cleaning up");
        pb_toniefile_close(info);
        return AEL_IO_DONE;
    }

    return bytes_read;
}

esp_err_t pb_seek(int32_t blocks)
{
    if (!pb_toniefile_info.valid)
    {
        return ESP_FAIL;
    }
    pb_toniefile_info.seek_blocks += blocks;
    return ESP_OK;
}

esp_err_t pb_set_chapter(int32_t chapter)
{
    if (!pb_toniefile_info.valid)
    {
        return ESP_FAIL;
    }
    pb_toniefile_info.target_chapter = chapter;
    return ESP_OK;
}

int32_t pb_get_chapter()
{
    if (!pb_toniefile_info.valid)
    {
        return ESP_FAIL;
    }
    return pb_toniefile_info.current_chapter;
}

esp_err_t pb_seek_chapter(int32_t chapters)
{
    if (!pb_toniefile_info.valid)
    {
        return ESP_FAIL;
    }
    int32_t target = pb_get_chapter() + chapters;

    if (target < 0)
    {
        target = 0;
    }
    if (target >= pb_toniefile_info.taf->n_track_page_nums)
    {
        target = pb_toniefile_info.taf->n_track_page_nums - 1;
    }
    return pb_set_chapter(target);
}

/* ********************************************************************************************************************* */
/* ********************************************************************************************************************* */
/* ********************************************************************************************************************* */

char *pb_build_filename(uint64_t id)
{
    char *filename = malloc(48);
    char *filename_ptr = filename;

    filename_ptr += sprintf(filename_ptr, "/sdcard/CONTENT/");

    for (int i = 0; i < 4; ++i)
    {
        uint8_t byte = (id >> (i * 8)) & 0xFF;
        filename_ptr += sprintf(filename_ptr, "%02X", byte);
    }
    filename_ptr += sprintf(filename_ptr, "/");
    for (int i = 4; i < 8; ++i)
    {
        uint8_t byte = (id >> (i * 8)) & 0xFF;
        filename_ptr += sprintf(filename_ptr, "%02X", byte);
    }
    return filename;
}

esp_err_t pb_play(const char *uri)
{
    ESP_LOGI(TAG, "[ * ] Queue uri: '%s'", uri);
    char *msg = strdup(uri);
    xQueueSend(playback_queue, &msg, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t pb_check_file(const char *filename)
{
    ESP_LOGI(TAG, "Check file '%s'", filename);

    /* no file at all, we have to download it */
#ifdef DEVBOARD
    ESP_LOGI(TAG, "devboard mode, fake response 'good'");
    return PB_ERR_GOOD_FILE;
#endif

    struct stat st;
    if (stat(filename, &st) != 0)
    {
        ESP_LOGE(TAG, "does not exist: '%s'", filename);
        return PB_ERR_NO_FILE;
    }

    if (st.st_size < TONIEFILE_FRAME_SIZE)
    {
        ESP_LOGE(TAG, "file size: %ld", st.st_size);
        return PB_ERR_EMPTY_FILE;
    }

    /* read details */
    FILE *fd = fopen(filename, "rb");
    if (!fd)
    {
        ESP_LOGE(TAG, "Failed to open file: '%s'", filename);
        return PB_ERR_NO_FILE;
    }

    TonieboxAudioFileHeader *taf = pb_toniefile_get_header(fd);
    fclose(fd);

    if (!taf)
    {
        ESP_LOGE(TAG, "Failed to read file: '%s'", filename);
        return PB_ERR_CORRUPTED_FILE;
    }

    esp_err_t ret = PB_ERR_GOOD_FILE;

    if (st.st_size != taf->num_bytes + TONIEFILE_FRAME_SIZE)
    {
        ESP_LOGW(TAG, "  TAF size: %llu, file size: %ld -> partial", taf->num_bytes, st.st_size);
        ret = PB_ERR_PARTIAL_FILE;
    }
    toniebox_audio_file_header__free_unpacked(taf, NULL);

    return ret;
}

esp_err_t pb_play_default(uint32_t id)
{
    pb_req_default_t *req = malloc(sizeof(pb_req_default_t));

    req->hdr.type = PB_REQ_TYPE_DEFAULT;
    req->voiceline = id;

    xQueueSend(playback_queue, &req, portMAX_DELAY);
    return ESP_OK;
}

/* when being called with the token, it depends on the play state what to do */
esp_err_t pb_play_content_token(uint64_t nfc_uid, const uint8_t *token)
{
    pb_req_play_token_t *req = malloc(sizeof(pb_req_play_token_t));

    req->hdr.type = PB_REQ_TYPE_PLAY_TOKEN;
    req->uid = nfc_uid;
    memcpy(req->token, token, 32);
    xQueueSend(playback_queue, &req, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t pb_play_content(uint64_t nfc_uid)
{
    pb_req_play_t *req = malloc(sizeof(pb_req_play_t));

    req->hdr.type = PB_REQ_TYPE_PLAY;
    req->uid = nfc_uid;

    xQueueSend(playback_queue, &req, portMAX_DELAY);
    return ESP_OK;
}

esp_err_t pb_stop()
{
    pb_req_stop_t *req = malloc(sizeof(pb_req_stop_t));

    req->hdr.type = PB_REQ_TYPE_STOP;

    xQueueSend(playback_queue, &req, portMAX_DELAY);
    return ESP_OK;
}

bool pb_is_playing()
{
    return pb_playing;
}

uint32_t pb_get_play_position()
{
    if (!pb_is_playing())
    {
        return 0;
    }

    return pb_toniefile_info.current_block;
}

uint64_t pb_get_current_uid()
{
    return pb_last_nfc_uid;
}

void pb_set_last(uint64_t nfc_uid, uint32_t play_position)
{
    pb_last_nfc_uid = nfc_uid;
    pb_last_play_position = play_position;
}

/********************************************************/
/* helpers for main loop, only to be called from there  */
/********************************************************/

static esp_err_t pb_int_abort_dl()
{
    /* do a semi-atomic swap, so other threads do not interfere */
    cloud_content_req_t *curr = current_dl_req;
    current_dl_req = NULL;

    if (curr)
    {
        ESP_LOGI(TAG, "There is a download for '%s'", curr->filename);
        curr->abort = true;
        while (curr->state < CC_STATE_FINISHED)
        {
            ESP_LOGI(TAG, "wait for abort...");
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        if (curr->handle)
        {
            fclose(curr->handle);
            curr->handle = NULL;
        }
        cloud_content_cleanup(curr);
    }

    return ESP_OK;
}

static void pb_int_stop()
{
    if (!pb_playing)
    {
        return;
    }
    audio_pipeline_pause(pipeline);
    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    pb_playing = false;
    pb_default_content = false;

    /* close local file handles and free stuff */
    pb_toniefile_close(&pb_toniefile_info);
    /* in case there is a download, also stop that one */
    pb_int_abort_dl();
}

static esp_err_t pb_int_play_file(const char *file, uint64_t nfc_uid)
{
    ESP_LOGI(TAG, "Play: '%s'", file);

    if (pb_toniefile_open(&pb_toniefile_info, file) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to play file: '%s'", file);
        return ESP_FAIL;
    }

    if (!pb_default_content)
    {
        if (nfc_uid == pb_last_nfc_uid)
        {
            ESP_LOGI(TAG, "UID %16llX match, set last play position to %lu", nfc_uid, pb_last_play_position);
            pb_toniefile_info.current_block = pb_last_play_position + 1;
        }
        else
        {
            ESP_LOGI(TAG, "UID %16llX is new, play from start", nfc_uid);
            pb_last_play_position = 0;
            pb_last_nfc_uid = nfc_uid;
        }
    }

    audio_pipeline_run(pipeline);
    audio_pipeline_resume(pipeline);

    return ESP_OK;
}

static esp_err_t pb_int_play_default(uint32_t lang, uint32_t id)
{
    char filename[64];

    sprintf(filename, "/sdcard/CONTENT/%08lX/%08lX", lang, id);
    if (pb_check_file(filename) == PB_ERR_GOOD_FILE)
    {
        pb_default_content = true;
        return pb_int_play_file(filename, 0);
    }
    lang = 0;

    sprintf(filename, "/sdcard/CONTENT/%08lX/%08lX", lang, id);
    if (pb_check_file(filename) == PB_ERR_GOOD_FILE)
    {
        pb_default_content = true;
        return pb_int_play_file(filename, 0);
    }
    ESP_LOGE(TAG, "requested ID does not exist: '%08lX', neither in lang %lu nor in default", id, lang);
    return ESP_ERR_NOT_FOUND;
}

/********************************************************/
/* handlers for main loop, only to be called from there */
/********************************************************/

static esp_err_t pb_req_handle_play(pb_req_play_t *req)
{
    pb_int_stop();

    char *filename = pb_build_filename(req->uid);
    esp_err_t file_state = pb_check_file(filename);

    /* right now we cannot play this file, wait for token to download it */
    if (file_state != PB_ERR_GOOD_FILE)
    {
        free(filename);
        ledman_change("checking");
        return ESP_FAIL;
    }

    pb_int_play_file(filename, req->uid);
    free(filename);

    return ESP_OK;
}

static esp_err_t pb_req_handle_play_token(pb_req_play_token_t *req)
{
    /* already playing? no need to download using token */
    if (pb_playing)
    {
        return ESP_OK;
    }

    char *filename = pb_build_filename(req->uid);
    esp_err_t file_state = pb_check_file(filename);

    /* quite unexpected. should not happen, but play anyway */
    if (file_state == PB_ERR_GOOD_FILE)
    {
        pb_int_play_file(filename, req->uid);
        free(filename);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "requested file shall be downloaded first: '%s'", filename);

    /* if there is already a download running, cancel it first */
    pb_int_abort_dl();

    ESP_LOGI(TAG, "initiate download");
    current_dl_req = cloud_content_download(req->uid, req->token);

    if (!pb_default_content && pb_last_nfc_uid != req->uid)
    {
        pb_last_play_position = 0;
    }

    bool proceed = false;
    bool waiting = true;
    uint32_t start_pos = (pb_default_content ? 0 : pb_last_play_position) + PB_MIN_DL_BLOCKS;

    while (waiting)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        switch (cloud_content_get_state(current_dl_req))
        {
        case CC_STATE_INIT:
        case CC_STATE_CONNECTING:
        case CC_STATE_ABORTED:
            break;

        case CC_STATE_CONNECTED:
            ESP_LOGI(TAG, "Connected...");
            break;

        case CC_STATE_ERROR:
            ESP_LOGE(TAG, "Download failed");
            waiting = false;
            proceed = false;
            current_dl_req = NULL;
            break;

        case CC_STATE_FINISHED:
            ESP_LOGI(TAG, "Download finished");
            waiting = false;
            proceed = true;
            current_dl_req = NULL;
            break;

        case CC_STATE_RECEIVING:
            if (current_dl_req->received / 4096 > start_pos)
            {
                ESP_LOGI(TAG, "Download in progress, enough blocks received");
                waiting = false;
                proceed = true;
            }
            break;
        }
    }

    if (!proceed)
    {
        ESP_LOGE(TAG, "...could not download the file");
        free(filename);
        ledman_change("failed");
        return ESP_ERR_NOT_FOUND;
    }

    return pb_int_play_file(filename, req->uid);
}

static esp_err_t pb_req_handle_stop(pb_req_stop_t *req)
{
    pb_int_stop();
    return ESP_OK;
}

static esp_err_t pb_req_handle_default(pb_req_default_t *req)
{
    pb_int_stop();
    return pb_int_play_default(0, req->voiceline);
}

/********************************************************/
/* main loop, calls functions above                     */
/********************************************************/

void pb_mainthread(void *arg)
{
    pb_playing = false;
    ESP_LOGI(TAG, "Listen for all pipeline events");

    while (1)
    {
        vTaskDelay(50 / portTICK_PERIOD_MS);

        if (board_headset_irq())
        {
            uint8_t type = dac3100_headset_detected();
            ESP_LOGI(TAG, "Headset detected: %s", type ? "YES" : "NO");
            dac3100_set_mute(!pb_is_playing() || type);
        }

        pb_req_t *req = NULL;
        if (xQueueReceive(playback_queue, &req, 0) == pdTRUE)
        {
            switch (req->type)
            {
            case PB_REQ_TYPE_PLAY:
                pb_req_handle_play((pb_req_play_t *)req);
                break;
            case PB_REQ_TYPE_PLAY_TOKEN:
                pb_req_handle_play_token((pb_req_play_token_t *)req);
                break;
            case PB_REQ_TYPE_STOP:
                pb_req_handle_stop((pb_req_stop_t *)req);
                break;
            case PB_REQ_TYPE_DEFAULT:
                pb_req_handle_default((pb_req_default_t *)req);
                break;
            default:
                break;
            }
            free(req);
        }

        audio_event_iface_msg_t msg;
        if (audio_event_iface_listen(evt, &msg, 0) == ESP_OK)
        {
            switch (msg.source_type)
            {
            default:
                ESP_LOGI(TAG, "[ * ] Receive info from %d", msg.source_type);
                break;
            case PERIPH_ID_SDCARD:
            {
                switch (msg.cmd)
                {
                case SDCARD_STATUS_UNKNOWN:
                    ESP_LOGI(TAG, "SDCARD_STATUS_UNKNOWN");
                    break;
                case SDCARD_STATUS_CARD_DETECT_CHANGE:
                    ESP_LOGI(TAG, "SDCARD_STATUS_CARD_DETECT_CHANGE");
                    break;
                case SDCARD_STATUS_MOUNTED:
                    ESP_LOGI(TAG, "SDCARD_STATUS_MOUNTED");
                    break;
                case SDCARD_STATUS_UNMOUNTED:
                    ESP_LOGI(TAG, "SDCARD_STATUS_UNMOUNTED");
                    break;
                case SDCARD_STATUS_MOUNT_ERROR:
                    ESP_LOGI(TAG, "SDCARD_STATUS_MOUNT_ERROR");
                    break;
                case SDCARD_STATUS_UNMOUNT_ERROR:
                    ESP_LOGI(TAG, "SDCARD_STATUS_UNMOUNT_ERROR");
                    break;
                }
                break;
            }
            case AUDIO_ELEMENT_TYPE_ELEMENT:
            {
                if (msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO)
                {
                    // msg.source == (void *)music_decoder &&
                    audio_element_info_t music_info = {0};
                    if (audio_element_getinfo(music_decoder, &music_info) == ESP_OK)
                    {
                        ESP_LOGI(TAG, "[ * ] Receive music info from decoder, sample_rates=%d, bits=%d, ch=%d",
                                 music_info.sample_rates, music_info.bits, music_info.channels);
                        audio_element_setinfo(i2s_stream_writer, &music_info);
                        if (i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels) != ESP_OK)
                        {
                            ESP_LOGE(TAG, "Failed to set I²S rate");
                        }
                    }
                }
                else if (msg.cmd == AEL_MSG_CMD_REPORT_STATUS)
                {
                    const char *source = "???";
                    if (msg.source == (void *)i2s_stream_writer)
                    {
                        source = "I²S";
                    }
                    if (msg.source == (void *)music_decoder)
                    {
                        source = "OPUS";
                    }

                    switch ((int)msg.data)
                    {
                    case AEL_STATUS_STATE_PAUSED:
                        ESP_LOGW(TAG, "[Event] [%s] Pause", source);
                        pb_playing = true;
                        dac3100_set_mute(true);
                        if (!pb_default_content)
                        {
                            ledman_change("idle");
                        }
                        break;
                    case AEL_STATUS_STATE_RUNNING:
                        ESP_LOGW(TAG, "[Event] [%s] Run", source);
                        pb_playing = true;
                        dac3100_set_mute(dac3100_headset_detected());
                        if (!pb_default_content)
                        {
                            if (current_dl_req)
                            {
                                ledman_change("playing download");
                            }
                            else
                            {
                                ledman_change("playing");
                            }
                        }
                        break;
                    case AEL_STATUS_STATE_STOPPED:
                    case AEL_STATUS_STATE_FINISHED:
                        ESP_LOGW(TAG, "[Event] [%s] Stop", source);
                        if (msg.source == (void *)i2s_stream_writer)
                        {
                            dac3100_set_mute(true);
                            if (!pb_default_content)
                            {
                                ledman_change("idle");
                            }
                            pb_default_content = false;
                            pb_playing = false;
                            audio_pipeline_reset_ringbuffer(pipeline);
                            audio_pipeline_reset_elements(pipeline);
                            audio_pipeline_change_state(pipeline, AEL_STATE_INIT);

                            /* close handle and free stuff, if not done before */
                            pb_toniefile_close(&pb_toniefile_info);
                            /* stop any pending download and close file handles */
                            pb_int_abort_dl();
                        }
                        break;
                    default:
                        ESP_LOGW(TAG, "[Event] [%s] %d", source, (int)msg.data);
                        break;
                    }
                }
                break;
            }
            }
        }
    }
}

void pb_init(esp_periph_set_handle_t set)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    playback_queue = xQueueCreate(PB_QUEUE_SIZE, sizeof(char *));
    ESP_LOGI(TAG, "Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.use_apll = false;
    //i2s_cfg.i2s_config.dma_buf_count = 4;
    //i2s_cfg.i2s_config.dma_buf_len = 512;

    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    opus_decoder_cfg_t opus_dec_cfg = DEFAULT_OPUS_DECODER_CONFIG();
    opus_dec_cfg.stack_in_ext = false;
    opus_dec_cfg.task_prio = 20;
    opus_dec_cfg.task_core = 1;
    //opus_dec_cfg.out_rb_size = 5096;
    music_decoder = decoder_opus_init(&opus_dec_cfg);

    audio_pipeline_register(pipeline, music_decoder, "dec");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    const char *link_tag[2] = {"dec", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);
    audio_element_set_read_cb(music_decoder, &pb_toniefile_cbr, &pb_toniefile_info);

    ESP_LOGI(TAG, "Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    audio_pipeline_set_listener(pipeline, evt);
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    xTaskCreatePinnedToCore(pb_mainthread, "[TB] Playback", 3500, NULL, PB_TASK_PRIO, NULL, tskNO_AFFINITY);
}

void pb_deinit()
{
    ESP_LOGI(TAG, "Stop audio_pipeline");

    audio_pipeline_stop(pipeline);
    audio_pipeline_wait_for_stop(pipeline);
    audio_pipeline_terminate(pipeline);

    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, music_decoder);

    audio_event_iface_destroy(evt);

    audio_pipeline_remove_listener(pipeline);

    audio_pipeline_deinit(pipeline);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(music_decoder);
}
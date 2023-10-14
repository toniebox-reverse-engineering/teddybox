

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
#include "board.h"
#include "math.h"
#include "led.h"
#include "toniebox.pb.taf-header.pb-c.h"

audio_pipeline_handle_t pipeline;
audio_element_handle_t i2s_stream_writer, music_decoder;
audio_event_iface_handle_t evt;

static QueueHandle_t playback_queue;

static const char *TAG = "[PB]";

/* ********************************************************************************************************************* */
/* ********************************************************************************************************************* */
/* ********************************************************************************************************************* */

typedef struct
{
    bool valid;
    char *filename;
    FILE *fd;
    int32_t current_pos;
    int32_t target_pos;
    int32_t current_chapter;
    int32_t target_chapter;
    int32_t seek_blocks;
    TonieboxAudioFileHeader *taf;
} pb_toniefile_t;

pb_toniefile_t pb_toniefile_info;

TonieboxAudioFileHeader *pb_toniefile_get_header(FILE *fd)
{
    TonieboxAudioFileHeader *taf = NULL;
    uint8_t buffer[TONIEFILE_FRAME_SIZE];
    uint8_t proto_be[4];

    fseek(fd, 0, SEEK_SET);

    if (fread(proto_be, 4, 1, fd) != 1)
    {
        ESP_LOGE(TAG, "Failed to read header size");
        return NULL;
    }
    uint32_t proto_size = (proto_be[0] << 24) | (proto_be[1] << 16) | (proto_be[2] << 8) | proto_be[3];
    if (proto_size > TONIEFILE_FRAME_SIZE)
    {
        ESP_LOGE(TAG, "Failed to read header size");
        return NULL;
    }
    if (fread(buffer, proto_size, 1, fd) != 1)
    {
        ESP_LOGE(TAG, "Failed to read header");
        return NULL;
    }

    taf = toniebox_audio_file_header__unpack(NULL, proto_size, (const uint8_t *)buffer);
    if (!taf)
    {
        ESP_LOGE(TAG, "Failed to parse header");
        return NULL;
    }
    return taf;
}

esp_err_t pb_toniefile_open(pb_toniefile_t *info, const char *filepath)
{
    ESP_LOGI(TAG, "Open Toniefile: %s", filepath);

    memset(info, 0x00, sizeof(pb_toniefile_t));

    info->fd = fopen(filepath, "r");
    if (!info->fd)
    {
        ESP_LOGE(TAG, "Failed to read file: %s", filepath);
        return ESP_FAIL;
    }

    info->taf = pb_toniefile_get_header(info->fd);
    if (!info->taf)
    {
        fclose(info->fd);
        return ESP_FAIL;
    }
    info->current_pos = TONIEFILE_FRAME_SIZE;
    fseek(info->fd, info->current_pos, SEEK_SET);

    ESP_LOGI(TAG, "  Audio ID: %08X", info->taf->audio_id);
    ESP_LOGI(TAG, "  Size:     %08llX", info->taf->num_bytes);
    ESP_LOGI(TAG, "  Chapters: %d", info->taf->n_track_page_nums);
    for (int chap = 0; chap < info->taf->n_track_page_nums; chap++)
    {
        ESP_LOGI(TAG, "    %d: offset %08X", chap, info->taf->track_page_nums[chap]);
    }

    info->current_chapter = -1;
    info->target_chapter = -1;
    info->target_pos = -1;
    info->seek_blocks = 0;
    info->valid = true;

    return ESP_OK;
}

void pb_toniefile_close(pb_toniefile_t *info)
{
    FILE *fd = info->fd;
    info->valid = false;
    info->fd = NULL;
    fclose(fd);
    toniebox_audio_file_header__free_unpacked(info->taf, NULL);
    info->taf = NULL;
}

int pb_toniefile_cbr(audio_element_handle_t self, char *buffer, int len, TickType_t ticks_to_wait, void *context)
{
    pb_toniefile_t *info = (pb_toniefile_t *)context;

    if (!info || !info->valid || !info->fd)
    {
        ESP_LOGE(TAG, "Playback already finished, but was called again");
        return AEL_IO_DONE;
    }

    /* doing seeking here, saves us some semaphores*/
    if (info->target_chapter >= 0)
    {
        ESP_LOGI(TAG, "Set target chapter %d", info->target_chapter);
        if (info->target_chapter < info->taf->n_track_page_nums)
        {
            uint32_t block = 1 + info->taf->track_page_nums[info->target_chapter];
            uint32_t offset = block * TONIEFILE_FRAME_SIZE;

            ESP_LOGI(TAG, " -> block %d, offset %d", block, offset);
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
        ESP_LOGI(TAG, "Seek %d blocks, current offset 0x%08X, target 0x%08X", info->seek_blocks, info->current_pos, info->target_pos);
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
            fseek(info->fd, info->target_pos, SEEK_SET);
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
                    ESP_LOGI(TAG, "Current chapter: %d", info->current_chapter);
                }
                break;
            }
        }
    }

    int bytes_read = fread(buffer, 1, len, info->fd);

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

void pb_init(esp_periph_set_handle_t set)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    playback_queue = xQueueCreate(PB_QUEUE_SIZE, sizeof(char *));
    ESP_LOGI(TAG, "Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "Create i2s stream to write data to codec chip");
    i2s_stream_cfg_t i2s_cfg = I2S_STREAM_CFG_DEFAULT();
    i2s_cfg.type = AUDIO_STREAM_WRITER;
    i2s_cfg.i2s_config.use_apll = false;
    i2s_cfg.i2s_config.dma_buf_count = 8;
    i2s_cfg.i2s_config.dma_buf_len = 512;

    i2s_stream_writer = i2s_stream_init(&i2s_cfg);

    ESP_LOGI(TAG, "Create opus decoder");
    opus_decoder_cfg_t opus_dec_cfg = DEFAULT_OPUS_DECODER_CONFIG();
    opus_dec_cfg.stack_in_ext = false;
    opus_dec_cfg.task_prio = 100;
    music_decoder = decoder_opus_init(&opus_dec_cfg);

    ESP_LOGI(TAG, "Register all elements to audio pipeline");
    audio_pipeline_register(pipeline, music_decoder, "dec");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "Link it together [toniefile]-->music_decoder-->i2s_stream-->[codec_chip]");
    const char *link_tag[2] = {"dec", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 2);
    audio_element_set_read_cb(music_decoder, &pb_toniefile_cbr, &pb_toniefile_info);

    ESP_LOGI(TAG, "Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    xTaskCreatePinnedToCore(pb_mainthread, "pb_main", 8192, NULL,
                            PB_TASK_PRIO, NULL, tskNO_AFFINITY);
}

esp_err_t pb_play(const char *uri)
{
    ESP_LOGI(TAG, "[ * ] Queue uri: '%s'", uri);
    char *msg = strdup(uri);
    xQueueSend(playback_queue, &msg, portMAX_DELAY);

    return ESP_OK;
}

esp_err_t pb_play_default_lang(uint32_t lang, uint32_t id)
{
    char filename[64];

    sprintf(filename, "/sdcard/CONTENT/%08X/%08X", lang, id);
    struct stat st;
    if (stat(filename, &st) == 0)
    {
        return pb_play(filename);
    }
    lang = 0;
    sprintf(filename, "/sdcard/CONTENT/%08X/%08X", lang, id);
    if (stat(filename, &st) == 0)
    {
        return pb_play(filename);
    }
    ESP_LOGE(TAG, "requested ID does not exist: '%08X', neither in lang %d nor in default", id, lang);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t pb_play_default(uint32_t id)
{
    /* read language from NVS? */
    return pb_play_default_lang(0, id);
}

esp_err_t pb_play_content(uint32_t id)
{
    char filename[64];

    sprintf(filename, "/sdcard/CONTENT/%08X/%08X", id, 0x500304E0);
    struct stat st;
    if (stat(filename, &st) != 0)
    {
        ESP_LOGE(TAG, "requested file does not exist: '%s'", filename);
        led_set_rgb(100, 0, 0);
        return ESP_ERR_NOT_FOUND;
    }
    return pb_play(filename);
}

esp_err_t pb_stop()
{
    char *msg = NULL;
    xQueueSend(playback_queue, &msg, portMAX_DELAY);

    return ESP_OK;
}

void pb_mainthread(void *arg)
{
    bool playing = false;
    ESP_LOGI(TAG, "[ * ] Listen for all pipeline events");

    while (1)
    {
        audio_event_iface_msg_t msg;

        char *item = NULL;

        if (xQueueReceive(playback_queue, &item, 0) == pdTRUE)
        {
            ESP_LOGI(TAG, "STOP");
            if (playing)
            {
                audio_pipeline_pause(pipeline);
                audio_pipeline_stop(pipeline);
                audio_pipeline_wait_for_stop(pipeline);
                audio_pipeline_terminate(pipeline);

                pb_toniefile_close(&pb_toniefile_info);
                playing = false;
            }

            if (item)
            {
                ESP_LOGI(TAG, "Play: '%s'", item);
                if (pb_toniefile_open(&pb_toniefile_info, item) != ESP_OK)
                {
                    ESP_LOGE(TAG, "Failed to read file: '%s'", item);
                }
                else
                {
                    audio_pipeline_run(pipeline);
                    audio_pipeline_resume(pipeline);
                }
                free(item);
            }
        }

        if (audio_event_iface_listen(evt, &msg, 50 / portTICK_PERIOD_MS) == ESP_OK)
        {
            switch (msg.source_type)
            {
            default:
                ESP_LOGI(TAG, "[ * ] Receive info from %d", msg.source_type);
                break;
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
                    //  &&
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
                        playing = true;
                        led_set_rgb(0, 50, 50);
                        break;
                    case AEL_STATUS_STATE_RUNNING:
                        ESP_LOGW(TAG, "[Event] [%s] Run", source);
                        playing = true;
                        led_set_rgb(0, 50, 100);
                        break;
                    case AEL_STATUS_STATE_STOPPED:
                    case AEL_STATUS_STATE_FINISHED:
                        ESP_LOGW(TAG, "[Event] [%s] Stop", source);
                        if (msg.source == (void *)i2s_stream_writer)
                        {
                            led_set_rgb(0, 100, 0);
                            playing = false;
                            audio_pipeline_reset_ringbuffer(pipeline);
                            audio_pipeline_reset_elements(pipeline);
                            audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
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

void pb_deinit()
{
    ESP_LOGI(TAG, "[ 7 ] Stop audio_pipeline");

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
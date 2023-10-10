

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "sdkconfig.h"
#include "fatfs_stream.h"
#include "i2s_stream.h"
#include "audio_element.h"
#include "audio_pipeline.h"
#include "audio_event_iface.h"
#include "audio_common.h"
#include "opus_decoder.h"

#include "playback.h"
#include "board.h"

audio_pipeline_handle_t pipeline;
audio_element_handle_t fatfs_stream_reader, i2s_stream_writer, music_decoder;
audio_event_iface_handle_t evt;

static QueueHandle_t playback_queue;

static const char *TAG = "[PB]";

void pb_init(esp_periph_set_handle_t set)
{
    esp_log_level_set(TAG, ESP_LOG_INFO);

    playback_queue = xQueueCreate(PB_QUEUE_SIZE, sizeof(char *));
    ESP_LOGI(TAG, "Create audio pipeline for playback");
    audio_pipeline_cfg_t pipeline_cfg = DEFAULT_AUDIO_PIPELINE_CONFIG();
    pipeline = audio_pipeline_init(&pipeline_cfg);
    mem_assert(pipeline);

    ESP_LOGI(TAG, "Create fatfs stream to read data from sdcard");
    fatfs_stream_cfg_t fatfs_cfg = FATFS_STREAM_CFG_DEFAULT();
    fatfs_cfg.type = AUDIO_STREAM_READER;
    fatfs_stream_reader = fatfs_stream_init(&fatfs_cfg);

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
    audio_pipeline_register(pipeline, fatfs_stream_reader, "file");
    audio_pipeline_register(pipeline, music_decoder, "dec");
    audio_pipeline_register(pipeline, i2s_stream_writer, "i2s");

    ESP_LOGI(TAG, "Link it together [sdcard]-->fatfs_stream-->music_decoder-->i2s_stream-->[codec_chip]");
    const char *link_tag[3] = {"file", "dec", "i2s"};
    audio_pipeline_link(pipeline, &link_tag[0], 3);

    ESP_LOGI(TAG, "Set up  event listener");
    audio_event_iface_cfg_t evt_cfg = AUDIO_EVENT_IFACE_DEFAULT_CFG();
    evt = audio_event_iface_init(&evt_cfg);

    ESP_LOGI(TAG, "Listening event from all elements of pipeline");
    audio_pipeline_set_listener(pipeline, evt);

    ESP_LOGI(TAG, "Listening event from peripherals");
    audio_event_iface_set_listener(esp_periph_set_get_event_iface(set), evt);

    xTaskCreatePinnedToCore(pb_mainthread, "pb_main", 4096, NULL,
                            PB_TASK_PRIO, NULL, tskNO_AFFINITY);
}

void pb_play(const char *uri)
{
    ESP_LOGI(TAG, "[ * ] Queue uri: '%s'", uri);
    char *msg = strdup(uri);
    xQueueSend(playback_queue, &msg, portMAX_DELAY);
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
            ESP_LOGI(TAG, "[ * ] Set up uri: '%s'", item);
            if (playing)
            {
                audio_pipeline_stop(pipeline);
                audio_pipeline_wait_for_stop(pipeline);
                audio_pipeline_reset_ringbuffer(pipeline);
                audio_pipeline_reset_elements(pipeline);
                audio_pipeline_change_state(pipeline, AEL_STATE_INIT);
                playing = false;
            }
            audio_element_set_uri(fatfs_stream_reader, item);
            audio_pipeline_run(pipeline);
            free(item);
        }

        if (audio_event_iface_listen(evt, &msg, 50 / portTICK_PERIOD_MS) == ESP_OK)
        {
            switch (msg.source_type)
            {
            case AUDIO_ELEMENT_TYPE_ELEMENT:
            {
                if (msg.cmd == AEL_MSG_CMD_REPORT_MUSIC_INFO)
                {
                    // msg.source == (void *)music_decoder &&
                    audio_element_info_t music_info = {0};
                    audio_element_getinfo(music_decoder, &music_info);

                    ESP_LOGI(TAG, "[ * ] Receive music info from decoder, sample_rates=%d, bits=%d, ch=%d",
                             music_info.sample_rates, music_info.bits, music_info.channels);

                    audio_element_setinfo(i2s_stream_writer, &music_info);
                    i2s_stream_set_clk(i2s_stream_writer, music_info.sample_rates, music_info.bits, music_info.channels);
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
                    if (msg.source == (void *)fatfs_stream_reader)
                    {
                        source = "FAT";
                    }

                    switch ((int)msg.data)
                    {
                    case AEL_STATUS_STATE_PAUSED:
                        ESP_LOGW(TAG, "[Event] [%s] Pause", source);
                        playing = true;
                        gpio_set_level(LED_BLUE_GPIO, 1);
                        break;
                    case AEL_STATUS_STATE_RUNNING:
                        ESP_LOGW(TAG, "[Event] [%s] Run", source);
                        playing = true;
                        gpio_set_level(LED_BLUE_GPIO, 1);
                        break;
                    case AEL_STATUS_STATE_STOPPED:
                    case AEL_STATUS_STATE_FINISHED:
                        ESP_LOGW(TAG, "[Event] [%s] Stop", source);
                        if (msg.source == (void *)i2s_stream_writer)
                        {
                            gpio_set_level(LED_BLUE_GPIO, 0);
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

    audio_pipeline_unregister(pipeline, fatfs_stream_reader);
    audio_pipeline_unregister(pipeline, i2s_stream_writer);
    audio_pipeline_unregister(pipeline, music_decoder);

    // audio_event_iface_remove_listener(esp_periph_set_get_event_iface(set), evt);
    audio_event_iface_destroy(evt);

    audio_pipeline_remove_listener(pipeline);

    audio_pipeline_deinit(pipeline);
    audio_element_deinit(fatfs_stream_reader);
    audio_element_deinit(i2s_stream_writer);
    audio_element_deinit(music_decoder);
}
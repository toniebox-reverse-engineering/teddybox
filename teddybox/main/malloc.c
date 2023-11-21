
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "esp_debug_helpers.h"

#define MALLOC_BUFFERS 2

uint8_t malloc_buffers[MALLOC_BUFFERS][30 * 1024];
bool malloc_buffers_used[MALLOC_BUFFERS];

#define TAG "[malloc]"

void *teddybox_custom_malloc(size_t size)
{
    if (size >= 25000)
    {
        for (int buf = 0; buf < MALLOC_BUFFERS; buf++)
        {
            if (!malloc_buffers_used[buf])
            {
                malloc_buffers_used[buf] = true;
                // ESP_LOGW(TAG, "malloc(%zu) at %d", size, buf);
                // esp_backtrace_print(100);
                return malloc_buffers[buf];
            }
        }
        ESP_LOGW(TAG, "Failed to alloc(%zu)", size);
    }
    return NULL;
}

void *teddybox_custom_calloc(size_t n, size_t size)
{
    if (size >= 25000)
    {
        for (int buf = 0; buf < MALLOC_BUFFERS; buf++)
        {
            if (!malloc_buffers_used[buf])
            {
                malloc_buffers_used[buf] = true;
                memset(malloc_buffers[buf], 0x00, sizeof(malloc_buffers[buf]));
                // ESP_LOGW(TAG, "calloc(%zu) at %d", size, buf);
                // esp_backtrace_print(100);
                return malloc_buffers[buf];
            }
        }
        ESP_LOGW(TAG, "Failed to alloc(%zu)", size);
    }
    return NULL;
}

bool teddybox_custom_free(void *ptr)
{
    for (int buf = 0; buf < MALLOC_BUFFERS; buf++)
    {
        if (ptr == malloc_buffers[buf])
        {
            malloc_buffers_used[buf] = false;
            //ESP_LOGW(TAG, "free() at %d", buf);
            return true;
        }
    }

    return false;
}
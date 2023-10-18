
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

uint8_t opus_stack[30*1024];


void *teddybox_custom_malloc(size_t size)
{
    if(size == sizeof(opus_stack))
    {
        return opus_stack;
    }
    return NULL;
}

void *teddybox_custom_calloc(size_t n, size_t size)
{
    if(size == sizeof(opus_stack))
    {
        memset(opus_stack, 0x00, sizeof(opus_stack));
        return opus_stack;
    }
    return NULL;
}

bool teddybox_custom_free(void *ptr)
{
    if(ptr == opus_stack)
    {
        return true;
    }
    return false;
}
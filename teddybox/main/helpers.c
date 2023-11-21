
#include <string.h>
#include <stdio.h>
#include <stdarg.h>


char *custom_asprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    // Calculate the length of the final string
    va_list tmp_args;
    va_copy(tmp_args, args);
    int length = vsnprintf(NULL, 0, fmt, tmp_args);
    va_end(tmp_args);

    if (length < 0)
    {
        return NULL;
    }

    // Allocate memory for the new string
    char *new_str = malloc(length + 1); // Add 1 for the null terminator
    if (new_str == NULL)
    {
        return NULL;
    }

    // Format the new string
    vsnprintf(new_str, length + 1, fmt, args);

    va_end(args);

    return new_str;
}
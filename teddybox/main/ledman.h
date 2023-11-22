#pragma once


typedef enum
{
    SYSTEM_NORMAL,
    SYSTEM_OFFLINE,
    SYSTEM_LOWBATT,
    NUM_SYSTEM_STATES // Keep this last
} SystemState;


#define COMMAND_SET_COLOR 0
#define COMMAND_DELAY 1
#define COMMAND_FADE 2
#define COMMAND_END 3
#define COMMAND_LOOP 4


#define SET_COLOR(r, g, b)                        \
    {                                             \
        .type = COMMAND_SET_COLOR, .color = { r,  \
                                              g,  \
                                              b } \
    }

#define DELAY(ms)                              \
    {                                          \
        .type = COMMAND_DELAY, .delay = { ms } \
    }

#define FADE(r, g, b, duration, step)             \
    {                                             \
        .type = COMMAND_FADE, .fade = { r,        \
                                        g,        \
                                        b,        \
                                        duration, \
                                        step }    \
    }

#define END()               \
    {                       \
        .type = COMMAND_END \
    }

#define LOOP()               \
    {                        \
        .type = COMMAND_LOOP \
    }

typedef struct
{
    float r, g, b;
} ColorCommand;

typedef struct
{
    uint32_t ms;
} DelayCommand;

typedef struct
{
    float r, g, b;
    uint32_t duration;
    uint32_t step;
} FadeCommand;

typedef struct
{
    uint8_t type;
    union
    {
        ColorCommand color;
        DelayCommand delay;
        FadeCommand fade;
    };
} SequenceCommand;

typedef struct
{
    const char *name;
    const SequenceCommand *seq;
} State;


void ledman_init();
void ledman_change(const char *name);
void ledman_set_system_state(SystemState state);

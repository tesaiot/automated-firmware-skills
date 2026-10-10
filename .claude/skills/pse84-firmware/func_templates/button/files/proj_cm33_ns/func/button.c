/*******************************************************************************
* File Name : button.c
*
* Layer 2 - function. Counter-based debounce: a raw level must stay stable for
* BUTTON_DEBOUNCE_MS before the debounced state changes and an event is raised.
*******************************************************************************/
#include "button.h"

#include "board.h"

typedef struct
{
    bool     stable;     /* debounced state (true = pressed) */
    uint16_t count;      /* ms the raw level has differed from stable */
    bool     pending;    /* state changed, event not yet reported */
} button_state_t;

static button_state_t state[BOARD_MAX_BUTTONS];

void button_init(void)
{
    for (uint32_t i = 0U; i < button_count(); i++)
    {
        state[i].stable  = board_button_is_pressed(i);
        state[i].count   = 0U;
        state[i].pending = false;
    }
}

uint32_t button_count(void)
{
    uint32_t n = board_button_count();
    return (n > BOARD_MAX_BUTTONS) ? BOARD_MAX_BUTTONS : n;
}

bool button_is_pressed(uint32_t num)
{
    return (num != 0U) && (num <= button_count()) && state[num - 1U].stable;
}

button_evt_t button_poll(uint32_t *num)
{
    for (uint32_t i = 0U; i < button_count(); i++)
    {
        bool raw = board_button_is_pressed(i);
        if (raw == state[i].stable)
        {
            state[i].count = 0U;
        }
        else if (++state[i].count >= (BUTTON_DEBOUNCE_MS / BUTTON_POLL_MS))
        {
            state[i].stable  = raw;
            state[i].count   = 0U;
            state[i].pending = true;
        }
    }
    for (uint32_t i = 0U; i < button_count(); i++)
    {
        if (state[i].pending)
        {
            state[i].pending = false;
            *num = i + 1U;
            return state[i].stable ? BUTTON_EVT_PRESSED : BUTTON_EVT_RELEASED;
        }
    }
    return BUTTON_EVT_NONE;
}

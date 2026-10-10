/*******************************************************************************
* File Name : led.c
*
* Layer 2 - function. LED service (1-based LED numbers).
*******************************************************************************/
#include "led.h"

#include "board.h"

void led_init(void)
{
    for (uint32_t n = 1U; n <= led_count(); n++)
    {
        (void)led_set(n, false);
    }
}

uint32_t led_count(void)
{
    return board_led_count();
}

bool led_set(uint32_t num, bool on)
{
    if ((num == 0U) || (num > board_led_count()))
    {
        return false;
    }
    board_led_write(num - 1U, on);
    return true;
}

bool led_toggle(uint32_t num)
{
    return led_set(num, !led_get(num));
}

bool led_get(uint32_t num)
{
    return (num != 0U) && board_led_read(num - 1U);
}

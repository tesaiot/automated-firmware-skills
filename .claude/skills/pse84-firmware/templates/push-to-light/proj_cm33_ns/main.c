/*******************************************************************************
* File Name : main.c  (CM33 non-secure)
*
* Layer 3 - application. Wires the function layer together; no CYBSP_* or
* Cy_* calls here (those belong in board/).
*
* push-to-light demo:
*   - console commands over the debug UART (see help)
*   - BTNn press toggles LEDn and reports an EVT line
* Boot chain: secure M33 -> this image -> CM55 (started here, idles in sleep).
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "button.h"
#include "console.h"
#include "led.h"

#define APP_NAME        "push-to-light"
#define APP_VERSION     "1.0"

static void print_info(void)
{
    printf("INFO app=%s v=%s board=%s leds=%lu btns=%lu\r\n", APP_NAME, APP_VERSION,
           BOARD_NAME, (unsigned long)led_count(), (unsigned long)button_count());
    printf("READY\r\n");
}

static void print_leds(void)
{
    printf("LED");
    for (uint32_t n = 1U; n <= led_count(); n++)
    {
        printf(" led%lu=%d", (unsigned long)n, led_get(n) ? 1 : 0);
    }
    printf("\r\n");
}

static void print_buttons(void)
{
    printf("BTN");
    for (uint32_t n = 1U; n <= button_count(); n++)
    {
        printf(" btn%lu=%d", (unsigned long)n, button_is_pressed(n) ? 1 : 0);
    }
    printf("\r\n");
}

/* led <n> on|off|toggle */
static void cmd_led(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("ERR usage: led <n> on|off|toggle\r\n");
        return;
    }
    uint32_t n  = (uint32_t)strtoul(argv[1], NULL, 10);
    bool     ok;
    if (strcmp(argv[2], "on") == 0)          { ok = led_set(n, true); }
    else if (strcmp(argv[2], "off") == 0)    { ok = led_set(n, false); }
    else if (strcmp(argv[2], "toggle") == 0) { ok = led_toggle(n); }
    else
    {
        printf("ERR bad action '%s'\r\n", argv[2]);
        return;
    }
    if (ok)
    {
        printf("OK led%lu=%d\r\n", (unsigned long)n, led_get(n) ? 1 : 0);
    }
    else
    {
        printf("ERR no led%s (have %lu)\r\n", argv[1], (unsigned long)led_count());
    }
}

static void cmd_led_q(int argc, char *argv[])  { (void)argc; (void)argv; print_leds(); }
static void cmd_btn_q(int argc, char *argv[])  { (void)argc; (void)argv; print_buttons(); }
static void cmd_info(int argc, char *argv[])   { (void)argc; (void)argv; print_info(); }

static const console_cmd_t commands[] =
{
    { "info", "info                  - app/board info", cmd_info  },
    { "led",  "led <n> on|off|toggle - drive LEDn",     cmd_led   },
    { "led?", "led?                  - read back LEDs", cmd_led_q },
    { "btn?", "btn?                  - button states",  cmd_btn_q },
};

int main(void)
{
    board_init();
    led_init();
    button_init();
    console_init(commands, sizeof(commands) / sizeof(commands[0]));

    printf("\r\n[APP] %s %s on %s - type 'help'\r\n", APP_NAME, APP_VERSION, BOARD_NAME);
    board_start_cm55();
    print_info();

    for (;;)
    {
        uint32_t     num;
        button_evt_t evt = button_poll(&num);

        if (evt == BUTTON_EVT_PRESSED)
        {
            (void)led_toggle(num);   /* BTNn toggles LEDn when it exists */
            printf("EVT btn%lu=pressed led%lu=%d\r\n", (unsigned long)num,
                   (unsigned long)num, led_get(num) ? 1 : 0);
        }
        else if (evt == BUTTON_EVT_RELEASED)
        {
            printf("EVT btn%lu=released\r\n", (unsigned long)num);
        }
        console_poll();
        board_delay_ms(BUTTON_POLL_MS);
    }
}

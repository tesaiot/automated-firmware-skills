/*******************************************************************************
* File Name : main.c  (CM33 non-secure)
*
* Layer 3 - application. Wires the function layer together; no CYBSP_* or
* Cy_* calls here (those belong in board/).
*
* button-led: while button BTNn is held, LEDn is on; released -> off.
* Every edge is reported with the button's silkscreen name and pin, so the
* same image also identifies which physical button is which.
* Console: info, btn?, led?, buttons (name/pin table), help.
* pin-watch additions: periodic "BTN btn1=.. btn2=.. btn3=.." (report on|off,
* for the browser live view) and a pin watch that prints "PINCHG P<port>.<pin>=0|1"
* for any input change on any port (watch arm|disarm|reset|on|off) - finds the pins
* of buttons that are not in the board sheet.
* Boot chain: secure M33 -> this image -> CM55 (started here, idles in sleep).
*******************************************************************************/
#include <stdio.h>

#include "board.h"
#include "button.h"
#include "console.h"
#include "led.h"
#include "pinwatch.h"
#include <string.h>

#define APP_NAME        "pin-watch"
#define REPORT_MS       (250U)
#define APP_VERSION     "1.0"

static void print_info(void)
{
    printf("INFO app=%s v=%s board=%s leds=%lu btns=%lu\r\n", APP_NAME, APP_VERSION,
           BOARD_NAME, (unsigned long)led_count(), (unsigned long)button_count());
    printf("READY\r\n");
}

static void cmd_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    print_info();
}

static void cmd_buttons(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    for (uint32_t n = 1U; n <= button_count(); n++)
    {
        printf("BUTTON btn%lu name=%s pin=%s led=%s\r\n", (unsigned long)n,
               board_button_name(n - 1U), board_button_pin(n - 1U),
               (n <= led_count()) ? board_led_name(n - 1U) : "-");
    }
}

static void cmd_pins_q(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    for (uint32_t n = 1U; n <= button_count(); n++)
    {
        printf("PIN btn%lu %s %s\r\n", (unsigned long)n, board_button_pin(n - 1U),
               board_button_diag(n - 1U));
    }
}

static void cmd_btn_q(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("BTN");
    for (uint32_t n = 1U; n <= button_count(); n++)
    {
        printf(" btn%lu=%d", (unsigned long)n, button_is_pressed(n) ? 1 : 0);
    }
    printf("\r\n");
}

static bool report_on = true;
static bool watch_on  = true;

static void on_pinchg(uint32_t port, uint32_t pin, bool level, bool muted_now)
{
    printf("PINCHG P%lu.%lu=%u%s\r\n", (unsigned long)port, (unsigned long)pin,
           level ? 1U : 0U, muted_now ? " muted=noisy" : "");
}

static void cmd_report(int argc, char *argv[])
{
    if (argc > 1)
    {
        report_on = (0 == strcmp(argv[1], "on"));
    }
    printf("OK report=%s\r\n", report_on ? "on" : "off");
}

static void cmd_watch(int argc, char *argv[])
{
    const char *a = (argc > 1) ? argv[1] : "";
    if (0 == strcmp(a, "arm"))
    {
        (void)pinwatch_arm();
    }
    else if (0 == strcmp(a, "disarm"))
    {
        pinwatch_disarm();
    }
    else if (0 == strcmp(a, "reset"))
    {
        pinwatch_reset();
    }
    else if ((0 == strcmp(a, "on")) || (0 == strcmp(a, "off")))
    {
        watch_on = (0 == strcmp(a, "on"));
        pinwatch_reset();
    }
    printf("OK watch=%s armed=%lu muted=%lu\r\n", watch_on ? "on" : "off",
           (unsigned long)pinwatch_armed(), (unsigned long)pinwatch_muted());
}

static void cmd_led_q(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("LED");
    for (uint32_t n = 1U; n <= led_count(); n++)
    {
        printf(" led%lu=%d", (unsigned long)n, led_get(n) ? 1 : 0);
    }
    printf("\r\n");
}

static const console_cmd_t commands[] =
{
    { "info",    "info                  - app/board info",          cmd_info    },
    { "buttons", "buttons               - button name/pin/LED map", cmd_buttons },
    { "btn?",    "btn?                  - button states",           cmd_btn_q   },
    { "pins?",   "pins?                 - button pin config (diag)", cmd_pins_q  },
    { "led?",    "led?                  - read back LEDs",          cmd_led_q   },
    { "report",  "report on|off         - periodic BTN line",       cmd_report  },
    { "watch",   "watch arm|disarm|reset|on|off - pin watch (PINCHG lines)", cmd_watch },
};

int main(void)
{
    board_init();
    led_init();
    button_init();
    console_init(commands, sizeof(commands) / sizeof(commands[0]));

    printf("\r\n[APP] %s %s on %s - hold a button, its LED lights\r\n",
           APP_NAME, APP_VERSION, BOARD_NAME);
    board_start_cm55();
    pinwatch_reset();
    print_info();

    uint32_t next_report = board_millis() + REPORT_MS;
    for (;;)
    {
        uint32_t     num;
        button_evt_t evt = button_poll(&num);

        if (evt != BUTTON_EVT_NONE)
        {
            bool pressed = (evt == BUTTON_EVT_PRESSED);
            (void)led_set(num, pressed);           /* no LEDn -> nothing to light */
            printf("EVT btn%lu=%s name=%s pin=%s led%lu=%d\r\n", (unsigned long)num,
                   pressed ? "pressed" : "released", board_button_name(num - 1U),
                   board_button_pin(num - 1U), (unsigned long)num, led_get(num) ? 1 : 0);
        }
        if (watch_on)
        {
            pinwatch_poll(board_millis(), on_pinchg);
        }
        if (report_on && ((int32_t)(board_millis() - next_report) >= 0))
        {
            next_report = board_millis() + REPORT_MS;
            cmd_btn_q(0, NULL);
        }
        console_poll();
        board_delay_ms(BUTTON_POLL_MS);
    }
}

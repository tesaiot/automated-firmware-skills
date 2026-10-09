/*******************************************************************************
* File Name : main.c  (CM33 non-secure)
*
* Layer 3 - application. No CYBSP_* / Cy_* here.
*
* dual-core-ipc: CM33 owns the console; CM55 is a worker core that executes
* requests posted through a shared-memory mailbox (../shared/ipc_mailbox.h,
* func/ipc_client.c) - ping/compute and driving LED1/LED2. CM33 reads the LED
* pin back on its own side, proving both cores act on the same hardware.
* Ported from an earlier led-uart-ipc demo (there CM55 owned the UART;
* here the console stays on CM33 so the skill's serial tests apply unchanged).
*
* Commands: info | ipc? | ping <n> | led55 <1|2> on|off | led?
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "console.h"
#include "ipc_client.h"

#define APP_NAME            "dual-core-ipc"
#define APP_VERSION         "1.0"
#define CM55_READY_WAIT_MS  (500U)
#define HB_WINDOW_MS        (100U)

static const char *status_text(uint32_t st)
{
    switch (st)
    {
        case IPC_OK:          return "ok";
        case IPC_ERR_CMD:     return "cm55 bad cmd";
        case IPC_ERR_ARG:     return "cm55 bad arg";
        case IPC_ERR_TIMEOUT: return "cm55 timeout";
        default:              return "cm55 unknown status";
    }
}

static void print_info(void)
{
    printf("INFO app=%s v=%s board=%s cm55=%u leds55=2\r\n", APP_NAME, APP_VERSION,
           BOARD_NAME, ipc_client_ready() ? 1U : 0U);
    printf("READY\r\n");
}

static void cmd_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    print_info();
}

static void cmd_ipc(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t hb0 = ipc_client_heartbeat();
    board_delay_ms(HB_WINDOW_MS);
    uint32_t hb1 = ipc_client_heartbeat();
    printf("IPC ready=%u alive=%u hb_per_s=%lu\r\n", ipc_client_ready() ? 1U : 0U,
           (hb1 != hb0) ? 1U : 0U, (unsigned long)((hb1 - hb0) * (1000U / HB_WINDOW_MS)));
}

static void cmd_ping(int argc, char *argv[])
{
    uint32_t arg = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 0) : 0UL;
    uint32_t result = 0UL;
    uint32_t rtt = 0UL;
    uint32_t st = ipc_client_call(IPC_CMD_PING, arg, &result, &rtt);

    if (IPC_OK == st)
    {
        printf("OK ping arg=%lu result=%lu rtt_ms=%lu\r\n", (unsigned long)arg,
               (unsigned long)result, (unsigned long)rtt);
    }
    else
    {
        printf("ERR %s\r\n", status_text(st));
    }
}

static void cmd_led55(int argc, char *argv[])
{
    if (argc < 3)
    {
        printf("ERR usage: led55 <1|2> on|off\r\n");
        return;
    }
    uint32_t led = (uint32_t)strtoul(argv[1], NULL, 10);
    uint32_t on;
    if (0 == strcmp(argv[2], "on"))
    {
        on = 1UL;
    }
    else if (0 == strcmp(argv[2], "off"))
    {
        on = 0UL;
    }
    else
    {
        printf("ERR bad action '%s'\r\n", argv[2]);
        return;
    }

    uint32_t result = 0UL;
    uint32_t rtt = 0UL;
    uint32_t st = ipc_client_call(IPC_CMD_LED, (led << 8) | on, &result, &rtt);
    if (IPC_OK != st)
    {
        printf("ERR %s\r\n", status_text(st));
        return;
    }
    /* CM55 drove the pin; CM33 reads the same pin back */
    bool cm33 = board_led_read(led - 1U);
    printf("OK led%lu=%lu cm55=%lu cm33=%u rtt_ms=%lu\r\n", (unsigned long)led,
           (unsigned long)on, (unsigned long)result, cm33 ? 1U : 0U, (unsigned long)rtt);
}

static void cmd_led_query(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("LED");
    for (uint32_t i = 0U; i < board_led_count(); i++)
    {
        printf(" led%lu=%u", (unsigned long)(i + 1U), board_led_read(i) ? 1U : 0U);
    }
    printf("\r\n");
}

static const console_cmd_t commands[] =
{
    { "info",  "info                  - app/board info + cm55 state", cmd_info      },
    { "ipc?",  "ipc?                  - cm55 ready/alive, heartbeat rate", cmd_ipc  },
    { "ping",  "ping <n>              - cm55 returns n+1", cmd_ping                 },
    { "led55", "led55 <1|2> on|off    - cm55 drives LEDn", cmd_led55                },
    { "led?",  "led?                  - LED pins as read by cm33", cmd_led_query    },
};

int main(void)
{
    board_init();
    console_init(commands, sizeof(commands) / sizeof(commands[0]));

    printf("\r\n[APP] %s %s on %s\r\n", APP_NAME, APP_VERSION, BOARD_NAME);

    ipc_client_init();            /* must be valid before CM55 runs */
    board_start_cm55();

    uint32_t t0 = board_millis();
    while (!ipc_client_ready() && ((board_millis() - t0) < CM55_READY_WAIT_MS))
    {
    }
    if (!ipc_client_ready())
    {
        printf("ERR cm55 not ready after %u ms\r\n", (unsigned)CM55_READY_WAIT_MS);
    }
    print_info();

    for (;;)
    {
        console_poll();
    }
}

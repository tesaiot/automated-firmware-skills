/*******************************************************************************
* File Name : main.c  (CM33 non-secure)
*
* Layer 3 - application. No CYBSP_* / Cy_* here.
*
* lvgl-demo: CM33 owns the console and enables the graphics clocks before it
* starts CM55; CM55 runs LVGL bare metal on the DSI panel (proj_cm55/main.c).
* 'gfx' reads CM55's display / GPU / LVGL / touch status through the mailbox,
* one field per IPC call (IPC_CMD_GFX), so tests need nobody at the screen.
*
* Commands: info | ipc? | gfx | uptime55
*******************************************************************************/
#include <stdio.h>

#include "board.h"
#include "bsp_status.h"
#include "console.h"
#include "display_clocks.h"
#include "ipc_client.h"

#define APP_NAME            "lvgl-demo"
#define APP_VERSION         "1.0"
#define CM55_READY_WAIT_MS  (500U)
#define HB_WINDOW_MS        (100U)

/* app command shared with proj_cm55/main.c: arg = field, result = value */
#define IPC_CMD_GFX         (IPC_CMD_APP_BASE + 0UL)
enum
{
    GFX_STATE = 0, GFX_DISPLAY, GFX_GPU, GFX_TOUCH, GFX_IRQS, GFX_FLUSHES, GFX_FRAME_MS,
    GFX_TIMEOUTS, GFX_PRESSES, GFX_TOUCH_XY, GFX_RSLT, GFX_TOUCH_ERRS, GFX_CLICKS, GFX_SLIDER,
    GFX_SHOW_MS, GFX_FIELDS
};

/* bsp_status_t names as printed by bsp_status_str() on CM33 */
static const char *st_name(uint32_t v)
{
    return bsp_status_str((bsp_status_t)v);
}

static void print_info(void)
{
    printf("INFO app=%s v=%s board=%s cm55=%u\r\n", APP_NAME, APP_VERSION, BOARD_NAME,
           ipc_client_ready() ? 1U : 0U);
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

static void cmd_uptime55(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t v = 0UL, rtt = 0UL;
    uint32_t st = ipc_client_call(IPC_CMD_UPTIME, 0UL, &v, &rtt);
    if (IPC_OK == st)
    {
        printf("OK uptime55_ms=%lu cm33_ms=%lu\r\n", (unsigned long)v, (unsigned long)board_millis());
    }
    else
    {
        printf("ERR ipc status=%lu\r\n", (unsigned long)st);
    }
}

static void cmd_gfx(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t v[GFX_FIELDS];
    for (uint32_t f = 0U; f < GFX_FIELDS; f++)
    {
        uint32_t rtt = 0UL;
        uint32_t st = ipc_client_call(IPC_CMD_GFX, f, &v[f], &rtt);
        if (IPC_OK != st)
        {
            printf("ERR gfx field=%lu ipc status=%lu (cm55 busy starting the display?)\r\n",
                   (unsigned long)f, (unsigned long)st);
            return;
        }
    }
    printf("GFX state=%s display=%s gpu=%s touch=%s irqs=%lu flushes=%lu render_ms=%lu "
           "show_ms=%lu timeouts=%lu rslt=0x%08lX\r\n",
           (0U != v[GFX_STATE]) ? "run" : "init", st_name(v[GFX_DISPLAY]), st_name(v[GFX_GPU]),
           st_name(v[GFX_TOUCH]), (unsigned long)v[GFX_IRQS], (unsigned long)v[GFX_FLUSHES],
           (unsigned long)v[GFX_FRAME_MS], (unsigned long)v[GFX_SHOW_MS], (unsigned long)v[GFX_TIMEOUTS],
           (unsigned long)v[GFX_RSLT]);
    printf("TOUCH presses=%lu x=%lu y=%lu errors=%lu clicks=%lu slider=%lu\r\n",
           (unsigned long)v[GFX_PRESSES], (unsigned long)(v[GFX_TOUCH_XY] >> 16),
           (unsigned long)(v[GFX_TOUCH_XY] & 0xFFFFUL), (unsigned long)v[GFX_TOUCH_ERRS],
           (unsigned long)v[GFX_CLICKS], (unsigned long)v[GFX_SLIDER]);
}

static const console_cmd_t commands[] =
{
    { "info",     "info                  - app/board info + cm55 state", cmd_info },
    { "ipc?",     "ipc?                  - cm55 ready/alive, heartbeat rate", cmd_ipc },
    { "gfx",      "gfx                   - display/GPU/LVGL/touch status from cm55", cmd_gfx },
    { "uptime55", "uptime55              - cm55 millisecond tick", cmd_uptime55 },
};

int main(void)
{
    board_init();
    console_init(commands, sizeof(commands) / sizeof(commands[0]));

    printf("\r\n[APP] %s %s on %s\r\n", APP_NAME, APP_VERSION, BOARD_NAME);

    display_clocks_enable();      /* GFXSS clocks: before CM55 touches the display */
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

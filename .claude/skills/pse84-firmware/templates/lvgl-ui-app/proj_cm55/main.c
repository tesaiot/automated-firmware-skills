/*******************************************************************************
* File Name : main.c  (CM55)
*
* Layer 3 - application of the graphics core (bare metal). CM33 owns the
* console; this core runs LVGL (func/ui_port.h) with the UI generated from
* ui/ui.json by ui-designer's ui_gen.py (ui/ui_gen.c - never edit; your event
* code goes into ui/ui_actions.c) and serves the mailbox:
*   IPC_CMD_GFX  display / GPU / LVGL / touch status, one field per call
*   IPC_CMD_UI   the designed UI: counts, widget boxes/values, data get/set,
*                screen load, simulated click (shared/ui_ids.h, ui_gen_ipc)
*******************************************************************************/

#include "board55.h"
#include "display.h"
#include "touch.h"
#include "ui_port.h"
#include "ui_gen.h"
#include "ipc_mailbox.h"

/* app command: arg = field, result = value (shared with proj_cm33_ns/main.c) */
#define IPC_CMD_GFX         (IPC_CMD_APP_BASE + 0UL)
enum
{
    GFX_STATE = 0, GFX_DISPLAY, GFX_GPU, GFX_TOUCH, GFX_IRQS, GFX_FLUSHES, GFX_FRAME_MS,
    GFX_TIMEOUTS, GFX_PRESSES, GFX_TOUCH_XY, GFX_RSLT, GFX_TOUCH_ERRS, GFX_SHOW_MS, GFX_FIELDS
};

static volatile uint32_t state;     /* 0 = init running, 1 = running, 2 = display failed */

static uint32_t gfx_field(uint32_t f, uint32_t *v)
{
    const ui_stats_t *s = ui_stats();
    switch (f)
    {
        case GFX_STATE:      *v = state;                          break;
        case GFX_DISPLAY:    *v = (uint32_t)s->display;           break;
        case GFX_GPU:        *v = (uint32_t)s->gpu;               break;
        case GFX_TOUCH:      *v = (uint32_t)s->touch;             break;
        case GFX_IRQS:       *v = display_irq_count();            break;
        case GFX_FLUSHES:    *v = s->flushes;                     break;
        case GFX_FRAME_MS:   *v = s->render_ms;                   break;
        case GFX_SHOW_MS:    *v = s->show_ms;                     break;
        case GFX_TIMEOUTS:   *v = display_timeout_count();        break;
        case GFX_PRESSES:    *v = touch_press_count();            break;
        case GFX_TOUCH_XY:   *v = ((uint32_t)s->touch_x << 16) | s->touch_y; break;
        case GFX_RSLT:       *v = display_last_rslt();            break;
        case GFX_TOUCH_ERRS: *v = touch_error_count();            break;
        default:             return IPC_ERR_ARG;
    }
    return IPC_OK;
}

static void serve_mailbox(ipc_mailbox_t *mb, uint32_t *last_seq)
{
    uint32_t seq = mb->cmd_seq;
    if (seq == *last_seq)
    {
        return;
    }
    __DMB();                                  /* cmd/arg were written before cmd_seq */
    uint32_t result = 0UL;
    uint32_t status;
    switch (mb->cmd)
    {
        case IPC_CMD_PING:   result = mb->arg + 1UL; status = IPC_OK;  break;
        case IPC_CMD_UPTIME: result = board55_millis(); status = IPC_OK; break;
        case IPC_CMD_GFX:    status = gfx_field(mb->arg, &result);      break;
        case IPC_CMD_UI:
            /* the UI exists only when the display came up */
            status = (1U == state) ? ui_gen_ipc(mb->arg, &result) : IPC_ERR_CMD;
            break;
        default:             status = IPC_ERR_CMD;                      break;
    }
    mb->result = result;
    mb->status = status;
    __DMB();                                  /* reply visible before the ack */
    mb->ack_seq = seq;
    *last_seq = seq;
}

int main(void)
{
    board55_init();

    ipc_mailbox_t *mb = IPC_MAILBOX;
    while (IPC_MAGIC != mb->magic)            /* CM33 sets it before starting this core */
    {
    }
    uint32_t last_seq = mb->cmd_seq;
    mb->cm55_ready = IPC_MAGIC;               /* console works while the display starts */

    if (BSP_OK == ui_init())
    {
        ui_gen_create();
        state = 1U;
    }
    else
    {
        state = 2U;
    }

    for (;;)
    {
        uint32_t idle = ui_poll();
        uint32_t t0 = board55_millis();
        do
        {
            serve_mailbox(mb, &last_seq);
            mb->heartbeat++;
            __WFI();                          /* SysTick wakes every 1 ms */
        } while ((board55_millis() - t0) < idle);
    }
}

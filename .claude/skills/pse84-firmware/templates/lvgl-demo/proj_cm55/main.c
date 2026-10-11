/*******************************************************************************
* File Name : main.c  (CM55)
*
* Layer 3 - application of the graphics core (bare metal). CM33 owns the
* console; this core runs LVGL (func/ui_port.h) and serves the mailbox
* (../shared/ipc_mailbox.h): IPC_CMD_GFX returns one status field per call so
* CM33's 'gfx' command can prove display, GPU, LVGL and touch without anyone
* looking at the screen.
* Screen: title, a button with a click counter, a slider with its value, the
* last touch position and the frame counter (refreshed every 500 ms).
*******************************************************************************/

#include "board55.h"
#include "display.h"
#include "touch.h"
#include "ui_port.h"
#include "lvgl.h"
#include "ipc_mailbox.h"

/* app command: arg = field, result = value (shared with proj_cm33_ns/main.c) */
#define IPC_CMD_GFX         (IPC_CMD_APP_BASE + 0UL)
enum
{
    GFX_STATE = 0, GFX_DISPLAY, GFX_GPU, GFX_TOUCH, GFX_IRQS, GFX_FLUSHES, GFX_FRAME_MS,
    GFX_TIMEOUTS, GFX_PRESSES, GFX_TOUCH_XY, GFX_RSLT, GFX_TOUCH_ERRS, GFX_CLICKS, GFX_SLIDER,
    GFX_SHOW_MS, GFX_FIELDS
};

static volatile uint32_t state;     /* 0 = init running, 1 = running */
static uint32_t   clicks;
static int32_t    slider_value;
static lv_obj_t  *lbl_clicks, *lbl_slider, *lbl_touch, *lbl_stats;

static void btn_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    clicks++;
    lv_label_set_text_fmt(lbl_clicks, "Clicks: %lu", (unsigned long)clicks);
}

static void slider_cb(lv_event_t *e)
{
    slider_value = lv_slider_get_value(lv_event_get_target_obj(e));
    lv_label_set_text_fmt(lbl_slider, "Slider: %ld", (long)slider_value);
}

static void stats_timer_cb(lv_timer_t *t)
{
    LV_UNUSED(t);
    const ui_stats_t *s = ui_stats();
    lv_label_set_text_fmt(lbl_touch, "Touch: %s x=%u y=%u", s->touch_pressed ? "down" : "up",
                          (unsigned)s->touch_x, (unsigned)s->touch_y);
    lv_label_set_text_fmt(lbl_stats, "Frames: %lu  render %lu ms  display IRQs: %lu",
                          (unsigned long)s->flushes, (unsigned long)s->render_ms,
                          (unsigned long)display_irq_count());
}

static void build_screen(void)
{
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x101820), 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(title, "TESAIoT  LVGL 9.5  bare metal CM55");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 16);

    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 200, 70);
    lv_obj_align(btn, LV_ALIGN_LEFT_MID, 60, -40);
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Press me");
    lv_obj_center(btn_lbl);

    lbl_clicks = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_clicks, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_clicks, "Clicks: 0");
    lv_obj_align_to(lbl_clicks, btn, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

    lv_obj_t *slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 360);
    lv_obj_align(slider, LV_ALIGN_RIGHT_MID, -60, -40);
    lv_obj_add_event_cb(slider, slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lbl_slider = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_slider, lv_color_hex(0xFFFFFF), 0);
    lv_label_set_text(lbl_slider, "Slider: 0");
    lv_obj_align_to(lbl_slider, slider, LV_ALIGN_OUT_BOTTOM_MID, 0, 16);

    lbl_touch = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_touch, lv_color_hex(0x9AD0FF), 0);
    lv_obj_align(lbl_touch, LV_ALIGN_BOTTOM_LEFT, 20, -50);

    lbl_stats = lv_label_create(scr);
    lv_obj_set_style_text_color(lbl_stats, lv_color_hex(0x9AD0FF), 0);
    lv_obj_align(lbl_stats, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    (void)lv_timer_create(stats_timer_cb, 500, NULL);
}

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
        case GFX_CLICKS:     *v = clicks;                         break;
        case GFX_SLIDER:     *v = (uint32_t)slider_value;         break;
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
        build_screen();
    }
    state = 1U;

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

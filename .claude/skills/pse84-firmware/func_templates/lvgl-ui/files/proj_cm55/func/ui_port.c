/*******************************************************************************
* File Name : ui_port.c                       component: lvgl-ui
*
* Layer 2 - function (CM55). See ui_port.h. Own code; guidelines (read, never
* copied): Infineon mtb-example-psoc-edge-gfx-lvgl-demo lv_port_disp/indev,
* E84_TouchUI lvgl_port, TESAIoT SDK lv_port_disp.c (explicit stride, flush
* watchdog). FreeRTOS notify replaced by display-dsi's frame-shown flag.
*******************************************************************************/
#include "ui_port.h"

#include "lvgl.h"
#include "board55.h"
#include "display.h"
#include "touch.h"

#define TOUCH_READ_PERIOD_MS    (30U)
#define MAX_IDLE_MS             (5U)

static ui_stats_t   stats;
static uint32_t     refr_t0;

static uint32_t tick_cb(void)
{
    return board55_millis();
}

static void refr_start_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    refr_t0 = board55_millis();
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    LV_UNUSED(area);
    LV_UNUSED(disp);
    stats.render_ms = board55_millis() - refr_t0;
    stats.flushes++;
    display_show(px_map);                   /* FULL mode: one flush per frame */
}

static void flush_wait_cb(lv_display_t *disp)
{
    /* LVGL calls this before it draws into the other buffer again */
    uint32_t t0 = board55_millis();
    (void)display_wait_shown(DISPLAY_SHOW_TIMEOUT_MS);
    stats.show_ms = board55_millis() - t0;
    lv_display_flush_ready(disp);
}

static void touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    LV_UNUSED(indev);
    bool pressed = false;
    uint16_t x = stats.touch_x, y = stats.touch_y;
    if (BSP_OK == touch_read(&pressed, &x, &y))
    {
        stats.touch_reads++;
        stats.touch_pressed = pressed;
        if (pressed)
        {
            stats.touch_x = x;
            stats.touch_y = y;
        }
    }
    /* on a bus error keep the last state (SDK guideline) */
    data->point.x = (int32_t)stats.touch_x;
    data->point.y = (int32_t)stats.touch_y;
    data->state   = stats.touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

bsp_status_t ui_init(void)
{
    stats.display = display_init();
    if (BSP_OK != stats.display)
    {
        stats.gpu = BSP_ERR_INIT;
        stats.touch = BSP_ERR_INIT;
        return stats.display;
    }
    stats.gpu   = display_gpu_init();
    stats.touch = touch_init();

    lv_init();
    lv_tick_set_cb(tick_cb);

    /* created at the buffer width, then the buffers with their 1664-byte stride,
     * then the visible 800x480 (the stride check would reject 800 + 1664) */
    lv_display_t *disp = lv_display_create((int32_t)DISPLAY_STRIDE_PX, (int32_t)DISPLAY_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers_with_stride(disp, display_framebuffer(0U), display_framebuffer(1U),
                                       DISPLAY_FB_BYTES, DISPLAY_STRIDE_BYTES,
                                       LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_resolution(disp, (int32_t)DISPLAY_WIDTH, (int32_t)DISPLAY_HEIGHT);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_display_set_flush_wait_cb(disp, flush_wait_cb);
    lv_display_add_event_cb(disp, refr_start_cb, LV_EVENT_REFR_START, NULL);

    if (BSP_OK == stats.touch)
    {
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(indev, touch_cb);
        lv_timer_set_period(lv_indev_get_read_timer(indev), TOUCH_READ_PERIOD_MS);
    }
    return BSP_OK;
}

uint32_t ui_poll(void)
{
    if (BSP_OK != stats.display)
    {
        return MAX_IDLE_MS;
    }
    uint32_t next = lv_timer_handler();
    return (next > MAX_IDLE_MS) ? MAX_IDLE_MS : next;
}

const ui_stats_t *ui_stats(void)
{
    return &stats;
}

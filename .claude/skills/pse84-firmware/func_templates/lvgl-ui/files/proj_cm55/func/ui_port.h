/*******************************************************************************
* File Name : ui_port.h                       component: lvgl-ui
*
* Layer 2 - function (CM55). LVGL 9.5 on display-dsi + touch-ft5406, bare metal:
* one context (main) calls ui_poll() in its loop; nothing LVGL runs in an ISR.
*   ui_init()   display_init + display_gpu_init + touch_init + lv_init, then an
*               LVGL display (FULL render mode, both frame buffers, stride 832 px,
*               resolution 800x480) and a pointer input device (touch, 30 ms).
*   ui_poll()   lv_timer_handler(); returns ms until LVGL needs the next call.
* Flush: display_show() swaps the frame buffer at the next frame end; the flush
* wait callback blocks until the display controller confirms it (500 ms timeout)
* so LVGL never draws into the buffer that is still being scanned out.
* The tick is board55_millis() (SysTick 1 kHz in bare metal).
*******************************************************************************/
#ifndef UI_PORT_H
#define UI_PORT_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

typedef struct
{
    bsp_status_t display;       /* display_init result */
    bsp_status_t gpu;           /* display_gpu_init result */
    bsp_status_t touch;         /* touch_init result */
    uint32_t     flushes;       /* frames handed to the display */
    uint32_t     render_ms;     /* last frame: refresh start -> flush (LVGL drawing) */
    uint32_t     show_ms;       /* last frame: buffer swap -> display confirmed it */
    uint32_t     touch_reads;
    bool         touch_pressed;
    uint16_t     touch_x, touch_y;
} ui_stats_t;

/* Returns the display result; LVGL runs without touch if only touch fails. */
bsp_status_t ui_init(void);
uint32_t     ui_poll(void);
const ui_stats_t *ui_stats(void);

#endif /* UI_PORT_H */

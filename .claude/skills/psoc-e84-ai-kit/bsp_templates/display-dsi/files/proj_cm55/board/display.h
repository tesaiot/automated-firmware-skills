/*******************************************************************************
* File Name : display.h                       component: display-dsi
*
* Layer 1 - BSP (CM55). MIPI-DSI display through the PSOC Edge graphics
* subsystem (GFXSS: display controller + VG-Lite GPU + MIPI-DSI host):
* Waveshare 4.3" DSI panel, 800x480 visible, driven as 832x480 RGB565 (the
* display controller needs a 128-byte aligned stride: 832 px * 2 B = 1664).
* Panel power/backlight: its MCU at I2C 0x45 on SCB5 (i2c-scb5-cm55), through
* Infineon's display-dsi-waveshare-4-3-lcd library.
*
* Two full frame buffers live in the BSP region gfx_mem (section .cy_gpu_buf,
* SOCMEM, mapped non-cacheable on CM55 by the BSP MPU setup -> no cache
* maintenance), together with the VG-Lite command heap (display_gpu_init()).
*
* Bare metal: display_show() hands a buffer to the display controller, which
* takes it at the next frame end and raises its interrupt; the ISR sets a flag
* (no RTOS notify). display_wait_shown() waits for it (WFI) with a timeout - a
* missed interrupt cannot freeze the caller (TESAIoT SDK guideline: 500 ms).
*
* Before CM55 starts, CM33_NS must enable the GFXSS clocks:
* display_clocks_enable() (proj_cm33_ns/board/display_clocks.h, same component).
*******************************************************************************/
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

#define DISPLAY_WIDTH           (800U)      /* visible pixels */
#define DISPLAY_HEIGHT          (480U)
#define DISPLAY_STRIDE_PX       (832U)      /* frame buffer line, pixels */
#define DISPLAY_BYTES_PP        (2U)        /* RGB565 */
#define DISPLAY_STRIDE_BYTES    (DISPLAY_STRIDE_PX * DISPLAY_BYTES_PP)
#define DISPLAY_FB_BYTES        (DISPLAY_STRIDE_BYTES * DISPLAY_HEIGHT)
#define DISPLAY_FB_COUNT        (2U)
#define DISPLAY_SHOW_TIMEOUT_MS (500U)

/* Starts SCB5 if needed (i2c5_init, 100 kHz), clears the I2C bus, starts GFXSS with
 * frame buffer 0 (filled black), enables the display-controller and SCB5
 * interrupts, then powers the panel and its backlight. */
bsp_status_t display_init(void);

/* VG-Lite GPU: hands it its command heap (in gfx_mem) and initialises it for
 * the full 832x480 target. Call after display_init(). */
bsp_status_t display_gpu_init(void);

void        *display_framebuffer(uint32_t index);       /* 0 or 1, 64-byte aligned */
void         display_show(const void *fb);              /* takes effect at frame end */
bool         display_shown(void);                       /* the last show took effect */
bool         display_wait_shown(uint32_t timeout_ms);   /* false = timed out (counted) */

bsp_status_t display_set_brightness(uint8_t percent);   /* 0..100 */

uint32_t     display_irq_count(void);       /* display-controller interrupts */
uint32_t     display_show_count(void);
uint32_t     display_timeout_count(void);   /* display_wait_shown timeouts */
uint32_t     display_last_rslt(void);       /* last GFXSS / I2C / VG-Lite result */

#endif /* DISPLAY_H */

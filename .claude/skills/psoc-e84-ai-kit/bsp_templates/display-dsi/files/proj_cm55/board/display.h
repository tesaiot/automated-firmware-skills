/*******************************************************************************
* File Name : display.h                       component: display-dsi (planned)
*
* Layer 1 - BSP, CM55. MIPI-DSI display of the PSOC Edge E84 AI Kit:
*   GFXSS (DC + GPU + MIPI-DSI host, 2 lanes, up to 1.5 Gbps/lane) on the
*   Raspberry-Pi compatible 15-pin connector J10 (3.3 V, SCL/SDA = SCB5 P17.0/
*   P17.1). Default panel: Waveshare 4.3" 800x480 DSI (panel MCU @0x45 for
*   power/backlight) + FT5406 capacitive touch @0x38 on J16 (5 V + I2C,
*   4-pin header NOT fitted by default; UG table 11 also lists 2.8/4/5/7").
* Backend: display-dsi-waveshare-4-3-lcd (mtb_disp_waveshare_4p3_init /
*   _set_brightness, PDL SCB5) + touch-ctp-ft5406 (mtb_ctp_ft5406_init /
*   _get_single_touch / _get_multi_touch) + PDL Cy_GFXSS_*, as in
*   mtb-example-psoc-edge-gfx-lvgl-demo (USE_KIT_PSE84_AI -> CYBSP_I2C_DISPLAY_*).
*   COMPONENTS+=GFXSS in common.mk; GFXSS clock on CM33 before CM55 boots.
*   Frame buffers in the BSP "gfx_mem" SoCMEM region.
*******************************************************************************/
#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

typedef enum
{
    DISP_FMT_RGB565 = 0,
    DISP_FMT_RGB888
} disp_format_t;

typedef struct
{
    uint16_t      width;          /* 800 */
    uint16_t      height;         /* 480 */
    disp_format_t format;         /* RGB565 default */
    bool          double_buffer;
} disp_config_t;

#define DISP_CONFIG_DEFAULT  { 800U, 480U, DISP_FMT_RGB565, true }

/* Panel power-up over I2C, GFXSS + DSI init, backlight on. */
bsp_status_t disp_init(const disp_config_t *cfg);
void         disp_deinit(void);
uint16_t     disp_width(void);
uint16_t     disp_height(void);

void        *disp_draw_buffer(void);           /* back buffer to render into */
bsp_status_t disp_flip(void);                  /* present at next vsync      */
bsp_status_t disp_wait_vsync(uint32_t timeout_ms);
bsp_status_t disp_set_brightness(uint8_t pct); /* 0..100                     */
bsp_status_t disp_fill(uint32_t rgb888);       /* test pattern helper        */

/* Touch (FT5406). pressed=false when no finger; x/y in panel pixels. */
bsp_status_t touch_init(void);
bsp_status_t touch_read(bool *pressed, uint16_t *x, uint16_t *y);

uint32_t     disp_last_rslt(void);

#endif /* DISPLAY_H */

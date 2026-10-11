/*******************************************************************************
* File Name : lv_conf.h                       component: lvgl-ui
*
* LVGL 9.5 configuration for PSOC Edge E84 CM55, bare metal (LV_OS_NONE),
* RGB565 on the display-dsi frame buffers, VG-Lite GPU draw unit + one software
* draw unit (text is drawn in software: see the lvgl-ui patches). Only settings
* that differ from LVGL's defaults (lv_conf_internal.h) are listed. Found via
* LV_CONF_INCLUDE_SIMPLE (DEFINES in the CM55 Makefile).
* Values follow Infineon mtb-example-psoc-edge-gfx-lvgl-demo (read, not copied).
*******************************************************************************/
#ifndef LV_CONF_H
#define LV_CONF_H

/* Colors / memory: the C library heap of the CM55 project */
#define LV_COLOR_DEPTH                  16
#define LV_USE_STDLIB_MALLOC            LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING            LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF           LV_STDLIB_CLIB

/* Timing / OS: bare metal, ui_poll() runs lv_timer_handler() from main */
#define LV_USE_OS                       LV_OS_NONE
#define LV_DEF_REFR_PERIOD              33      /* ms; SDK: faster refresh made the backlight band flicker */
#define LV_DPI_DEF                      130

/* Draw buffers: 64-byte aligned (VG-Lite) */
#define LV_DRAW_BUF_ALIGN               64
#define LV_DRAW_BUF_STRIDE_ALIGN        1
#define LV_DRAW_LAYER_SIMPLE_BUF_SIZE   (64 * 1024)

/* Software draw unit (text, anything the GPU unit declines) */
#define LV_USE_DRAW_SW                  1
#define LV_DRAW_SW_DRAW_UNIT_CNT        1
#define LV_USE_DRAW_SW_ASM              LV_DRAW_SW_ASM_NONE
#define LV_DRAW_SW_COMPLEX              1

/* VG-Lite GPU draw unit: driver from mtb-dsl-pse8xxgp (GFXSS), GPU initialised by
 * display_gpu_init() before lv_init() */
#define LV_USE_FLOAT                    1       /* VG-Lite needs the matrix code */
#define LV_USE_MATRIX                   1
#define LV_USE_DRAW_VG_LITE             1
#define LV_VG_LITE_USE_GPU_INIT         0
#define LV_VG_LITE_USE_ASSERT           0
#define LV_VG_LITE_FLUSH_MAX_COUNT      8
#define LV_VG_LITE_USE_BOX_SHADOW       1
#define LV_VG_LITE_GRAD_CACHE_CNT       32
#define LV_VG_LITE_STROKE_CACHE_CNT     32

/* No console on CM55 (CM33 owns the UART) */
#define LV_USE_LOG                      0
#define LV_USE_ASSERT_NULL              1
#define LV_USE_ASSERT_MALLOC            1

/* Fonts used by the demo screen */
#define LV_FONT_MONTSERRAT_14           1
#define LV_FONT_MONTSERRAT_24           1
#define LV_FONT_DEFAULT                 &lv_font_montserrat_14

/* Off: demos, examples, OS-dependent and file-system extras */
#define LV_USE_PERF_MONITOR             0
#define LV_USE_MEM_MONITOR              0
#define LV_BUILD_EXAMPLES               0
#define LV_BUILD_DEMOS                  0

/* Apps whose UI is generated (ui-designer ui_gen.py) enable the fonts it uses here */
#if defined(__has_include)
#if __has_include("ui_conf.h")
#include "ui_conf.h"
#endif
#endif

#endif /* LV_CONF_H */

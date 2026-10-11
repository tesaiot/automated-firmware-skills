/*******************************************************************************
* File Name : display_clocks.h                component: display-dsi (CM33_NS part)
*
* Layer 1 - BSP (CM33_NS). The vendor BSP 1.4.0 has no GFXSS block, so nothing
* enables the graphics subsystem's peripheral clocks (GPU, display controller,
* MIPI-DSI). Call display_clocks_enable() on CM33_NS BEFORE board_start_cm55();
* CM55 then runs display_init() (proj_cm55/board/display.h).
* Guideline: TESAIoT SDK proj_cm33_ns/main.c (same three calls before CM55 boots).
*******************************************************************************/
#ifndef DISPLAY_CLOCKS_H
#define DISPLAY_CLOCKS_H

void display_clocks_enable(void);

#endif /* DISPLAY_CLOCKS_H */

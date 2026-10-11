/*******************************************************************************
* File Name : touch.h                         component: touch-ft5406
*
* Layer 1 - BSP (CM55). FT5406 capacitive touch of the Waveshare DSI panels,
* I2C 0x38 on SCB5 (i2c-scb5-cm55), through Infineon's touch-ctp-ft5406
* library (interrupt-driven PDL transfers -> i2c5_irq_enable(), done by
* display_init()). Single touch only (the library's multi-touch call clears its
* result array before returning - do not use it).
*
* Coordinates are display pixels, origin top-left, already mirrored: the panel
* reports them rotated by 180 degrees (all three references flip x and y).
* A read blocks ~2 ms on the bus; poll every 20..50 ms from one context.
*******************************************************************************/
#ifndef TOUCH_H
#define TOUCH_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

bsp_status_t touch_init(void);                                  /* after display_init() */
bsp_status_t touch_read(bool *pressed, uint16_t *x, uint16_t *y);

uint32_t     touch_press_count(void);       /* released -> pressed transitions */
uint32_t     touch_error_count(void);
uint32_t     touch_last_rslt(void);

#endif /* TOUCH_H */

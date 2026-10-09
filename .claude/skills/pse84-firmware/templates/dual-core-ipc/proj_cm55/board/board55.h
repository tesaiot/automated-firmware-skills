/*******************************************************************************
* File Name : board55.h
*
* Layer 1 - board layer of the CM55 project. The only CM55 code that uses
* CYBSP_* / Cy_*. Kept tiny: CM55 is the worker core here; the console, buttons
* and the full board API live on CM33 (templates/_common/proj_cm33_ns/board).
*******************************************************************************/
#ifndef BOARD55_H
#define BOARD55_H

#include <stdbool.h>
#include <stdint.h>

#define BOARD55_LEDS        (2U)    /* LED1, LED2 exist on every supported BSP */

void     board55_init(void);                     /* cybsp_init + IRQs        */
bool     board55_led_write(uint32_t led, bool on); /* led 1..BOARD55_LEDS    */
bool     board55_led_read(uint32_t led);         /* output register readback */
void     board55_delay_us(uint32_t us);
void     board55_fatal(void);

#endif /* BOARD55_H */

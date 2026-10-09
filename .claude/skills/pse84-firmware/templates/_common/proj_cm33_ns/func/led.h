/*******************************************************************************
* File Name : led.h
*
* Layer 2 - function. LED service on top of board.h: 1-based numbering as
* printed on the board (LED1..LEDn), toggle, and state read-back.
*******************************************************************************/
#ifndef LED_H
#define LED_H

#include <stdbool.h>
#include <stdint.h>

void     led_init(void);
uint32_t led_count(void);
bool     led_set(uint32_t num, bool on);     /* false if num is out of range */
bool     led_toggle(uint32_t num);
bool     led_get(uint32_t num);               /* read back from the pin       */

#endif /* LED_H */

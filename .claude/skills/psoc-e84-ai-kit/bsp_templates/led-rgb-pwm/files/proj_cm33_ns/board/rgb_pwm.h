/*******************************************************************************
* File Name : rgb_pwm.h                       component: led-rgb-pwm (planned)
*
* Layer 1 - BSP. Colour control of RGB LED3 of the PSOC Edge E84 AI Kit:
*   red P20.6, green P20.4, blue P20.5 (active high, 1.8 V), driven by TCPWM0
*   group 1 PWM lines 265 / 263 / 264 (BSP routing; CYBSP_PWM_LED_CTRL is
*   TCPWM0 counter 0 - check the line numbers in the BSP before use).
* Pin ownership: ai-kit-board drives P20.4-6 as GPIO LEDs (claims them), so the
*   gate blocks this component until ai-kit-board gets a BOARD_RGB_PWM switch
*   that drops the RGB entries from its LED table (backlog).
*******************************************************************************/
#ifndef RGB_PWM_H
#define RGB_PWM_H

#include <stdint.h>
#include "bsp_status.h"

bsp_status_t rgb_init(uint32_t pwm_hz);              /* default 1000 Hz      */
void         rgb_deinit(void);                       /* pins back to GPIO off */
bsp_status_t rgb_set(uint8_t r, uint8_t g, uint8_t b);  /* 0..255 each        */
bsp_status_t rgb_set_hex(uint32_t rrggbb);

#endif /* RGB_PWM_H */

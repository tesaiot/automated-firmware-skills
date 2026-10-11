/*******************************************************************************
* File Name : pinwatch.h   (template pin-watch, diagnostic)
*
* Layer 1 - board diagnostic. Finds which MCU pin a physical button is on:
* watches the input register of all 22 GPIO ports and reports every change.
* pinwatch_arm() turns on a plain digital input (high-Z, no pull, no drive) on
* pins the BSP left at reset default (HSIOM = GPIO, analog/off) - except the
* header ports (P9.0-P9.3, P13, P15), mic inputs (P14) and crystals (P18, P19),
* which stay untouched. Configured pins are never changed; only GPIO-mode pins report.
* Pins that toggle more than PINWATCH_NOISY_PER_S times a second are muted.
*******************************************************************************/
#ifndef PINWATCH_H
#define PINWATCH_H

#include <stdbool.h>
#include <stdint.h>

#define PINWATCH_NOISY_PER_S    (10U)

typedef void (*pinwatch_cb_t)(uint32_t port, uint32_t pin, bool level, bool muted_now);

uint32_t pinwatch_arm(void);      /* returns number of pins switched to digital input */
void     pinwatch_disarm(void);   /* armed pins back to reset default (analog/off)    */
uint32_t pinwatch_armed(void);
void     pinwatch_reset(void);    /* new snapshot, clear mutes                        */
void     pinwatch_poll(uint32_t now_ms, pinwatch_cb_t cb);
uint32_t pinwatch_muted(void);

#endif /* PINWATCH_H */

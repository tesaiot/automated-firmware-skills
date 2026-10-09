/*******************************************************************************
* File Name : button.h
*
* Layer 2 - function. Debounced buttons with press/release events.
* Call button_poll() periodically (every BUTTON_POLL_MS); it returns events.
*******************************************************************************/
#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>
#include <stdint.h>

#define BUTTON_POLL_MS          (1U)
#define BUTTON_DEBOUNCE_MS      (20U)

typedef enum
{
    BUTTON_EVT_NONE = 0,
    BUTTON_EVT_PRESSED,
    BUTTON_EVT_RELEASED
} button_evt_t;

void         button_init(void);
uint32_t     button_count(void);
bool         button_is_pressed(uint32_t num);          /* debounced, 1-based */
/* Advance debouncing by one tick; reports at most one event per call.
 * On an event, *num is set to the 1-based button number. */
button_evt_t button_poll(uint32_t *num);

#endif /* BUTTON_H */

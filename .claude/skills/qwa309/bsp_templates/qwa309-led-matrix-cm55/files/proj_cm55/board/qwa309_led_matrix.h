/*******************************************************************************
* File Name : qwa309_led_matrix.h             component: qwa309-led-matrix
*
* Layer 1 - BSP (QWA309 base board, board id tesaiot only). DFRobot DFR0522
* 16x8 RGB LED matrix at I2C 0x10 on SCB5 (i2c-scb5).
*
* The panel is a register file: [0x02 FUNC, 0x03 COLOR, 0x04 X, 0x05 Y], acted
* on at the end of the write. Every command is 5 bytes ([reg, FUNC, COLOR, X, Y];
* the DFRobot library pads to 51 - not needed, verified by the SDK team). There is no frame upload, so
* ledm_frame() diffs against a shadow buffer and sends only changed pixels.
* Colours are 3-bit: 0 off, 1 red, 2 green, 3 yellow, 4 blue, 5 purple, 6 cyan,
* 7 white. x = 0..15 (left to right), y = 0..7 (top to bottom).
*
* Blocking calls on the caller's context; under FreeRTOS use them from one task
* (or install the i2c5 lock). No timers inside - animate from the app loop:
* ledm_text() renders one marquee step into a frame for ledm_frame().
*******************************************************************************/
#ifndef QWA309_LED_MATRIX_H
#define QWA309_LED_MATRIX_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

#define LEDM_WIDTH      (16U)
#define LEDM_HEIGHT     (8U)
#define LEDM_PIXELS     (LEDM_WIDTH * LEDM_HEIGHT)

typedef enum
{
    LEDM_OFF = 0U, LEDM_RED, LEDM_GREEN, LEDM_YELLOW, LEDM_BLUE, LEDM_PURPLE,
    LEDM_CYAN, LEDM_WHITE
} ledm_color_t;

/* needs i2c5_init() first; clears the panel (= known shadow state) */
bsp_status_t ledm_init(void);

bsp_status_t ledm_clear(void);
bsp_status_t ledm_fill(uint8_t color);                       /* 1..7 */
bsp_status_t ledm_pixel(uint8_t x, uint8_t y, uint8_t color);/* 0..7 */

/* frame = 128 bytes, row-major (frame[y * 16 + x] = colour 0..7); sends only
 * the pixels that differ from the shadow; *sent = commands written (may be NULL) */
bsp_status_t ledm_frame(const uint8_t frame[LEDM_PIXELS], uint32_t *sent);

/* helpers that render into a caller frame (no I2C) */
void ledm_digits(uint8_t frame[LEDM_PIXELS], uint16_t value, uint8_t color);   /* 0..9999, 3x5 font, right aligned */
void ledm_bar(uint8_t frame[LEDM_PIXELS], uint32_t value, uint32_t max, uint8_t color); /* bottom row */
/* marquee step: text enters from the right edge, offset = step count (0..),
 * returns false once the text has left the panel (restart offset at 0) */
bool ledm_text(uint8_t frame[LEDM_PIXELS], const char *text, uint32_t offset, uint8_t color);

uint32_t ledm_last_rslt(void);

#endif /* QWA309_LED_MATRIX_H */

/*******************************************************************************
* File Name : qwa309_capsense.h               component: qwa309-capsense
*
* Layer 1 - BSP (QWA309 base board, board id tesaiot only). CapSense pads handled
* by a PSoC 4000T that the E84 only sees as an I2C target: EZI2C @0x08 on the
* 3.3 V SCB5 bus (i2c_scb5.h). Needs the QWA309 CS select switch ON.
*
* The 4000T firmware differs between boards, so capsense_init() detects one of
* two buffer formats (plain read, no register address):
*   CAPSENSE_FMT_FRAME3 - TESAIoT SDK protocol, 3 bytes [btn0][btn1][slider 0..100].
*     Button bytes are ASCII digits, legacy 30..32 or numeric 0..2; pressed = code
*     differs from the idle code captured at init.
*   CAPSENSE_FMT_RAW5 - CapSense data block (seen 2026-10-10): bytes 6..15 = raw
*     counts of a 5-segment slider, left..right (16-bit LE, ~34000 idle, a touch
*     LOWERS the count by ~1500..5000). This firmware scans no buttons (btn0/btn1
*     stay false, has_buttons = false). The block's own baseline/diff words are
*     not used - that firmware drags its baseline down during a touch and then
*     reports a false touch; this driver keeps its own baseline, updated only
*     while no segment is touched. slider = centroid of the segment drops, 0..100.
* Do not touch the pads during capsense_init() / capsense_rebaseline().
*******************************************************************************/
#ifndef QWA309_CAPSENSE_H
#define QWA309_CAPSENSE_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

#define CAPSENSE_SEGMENTS       (5U)      /* RAW5 slider segments */
/* RAW5 thresholds on the drop (counts). Measured 2026-10-10: idle noise about +-30,
 * neighbours of a touched segment rise ~250, a touch drops 1500 (left end segment)
 * to 5000 (middle) - a light tap on the left end can stay below 700. */
#define CAPSENSE_TOUCH_ON       (350)     /* report a touch above this drop */
#define CAPSENSE_TOUCH_OFF      (200)     /* release below this drop */
#define CAPSENSE_INIT_TRIES     (10U)     /* 4000T may still boot after an E84 reset: 10 x 20 ms */

typedef enum
{
    CAPSENSE_FMT_NONE = 0,
    CAPSENSE_FMT_FRAME3,
    CAPSENSE_FMT_RAW5
} capsense_fmt_t;

typedef struct
{
    bool    btn0;               /* pressed (FRAME3 only) */
    bool    btn1;
    bool    has_buttons;        /* false for RAW5 */
    bool    slider_touched;     /* RAW5: a segment is touched; FRAME3: slider != 0 */
    uint8_t slider;             /* 0..100 (FRAME3 as sent; RAW5 centroid, held while untouched) */
    uint8_t raw[3];             /* FRAME3: last frame, for diagnostics */
    int16_t drop[CAPSENSE_SEGMENTS]; /* RAW5: own baseline - raw count, left..right */
} capsense_state_t;

/* Needs i2c5_init() first. Detects the format, captures the idle baseline. */
bsp_status_t   capsense_init(void);
bsp_status_t   capsense_rebaseline(void);          /* pads untouched */
bsp_status_t   capsense_read(capsense_state_t *s);
capsense_fmt_t capsense_format(void);
const char    *capsense_format_name(void);        /* "none", "frame3", "raw5" */
uint8_t        capsense_sdk_code(uint8_t raw);     /* SDK button-byte decode (diagnostics) */

uint32_t       capsense_last_rslt(void);

#endif /* QWA309_CAPSENSE_H */

/*******************************************************************************
* File Name : qwa309_capsense.c               component: qwa309-capsense
*
* Layer 1 - BSP (QWA309). PSoC 4000T CapSense controller (EZI2C @0x08 on SCB5).
* FRAME3 decoding follows the TESAIoT SDK guideline (common/capsense/src/
* capsense_decode.c, capsense_protocol.h - read, rewritten, never built here).
* RAW5 (5-segment slider raw counts at bytes 6..15) was mapped on hardware
* 2026-10-10 (left/middle/right touch -> segments 0/2/4); see qwa309 board sheet.
*******************************************************************************/
#include "qwa309_capsense.h"

#include <string.h>
#include "i2c_scb5.h"
#include "qwa309.h"            /* compile-time board-scope gate + I2C address */

#define RAW5_BYTES          (16U)       /* words 0..7: id, counter, status, 5 raw */
#define RAW5_FIRST          (6U)        /* byte offset of segment 0's raw count */
#define RAW5_MIN            (8000U)     /* plausible raw-count window for detection */
#define RAW5_MAX            (62000U)
#define RAW5_NOISE          (200)       /* |drop| below this on every segment = idle */
#define RAW5_WEIGHT_FLOOR   (100)       /* centroid ignores drops below this */
#define RAW5_RISE_READS     (40U)       /* all segments above baseline this long -> re-baseline */
#define BSL_SHIFT           (4U)        /* baseline IIR 1/16, kept in Q4 */

static capsense_fmt_t fmt;
static uint8_t  idle0, idle1;
static int32_t  bsl_q4[CAPSENSE_SEGMENTS];
static bool     touched;
static uint8_t  slider_last;
static uint32_t rise_reads;
static uint32_t last_rslt;

static uint8_t decode_button(uint8_t raw)
{
    if ((raw >= 0x30U) && (raw <= 0x39U))
    {
        return (uint8_t)(raw - 0x30U);          /* ASCII digit */
    }
    if ((raw >= 30U) && (raw <= 32U))
    {
        return (uint8_t)(raw - 30U);            /* legacy encoding seen on some units */
    }
    if (raw <= 2U)
    {
        return raw;                             /* numeric */
    }
    return 1U;
}

static bsp_status_t read_bytes(uint8_t *b, uint32_t n)
{
    bsp_status_t s = i2c5_read(QWA309_I2C_ADDR_CAPSENSE, b, n);
    last_rslt = i2c5_last_rslt();
    return s;
}

static uint16_t seg_raw(const uint8_t *b, uint32_t i)
{
    return (uint16_t)((uint16_t)b[RAW5_FIRST + (2U * i)] |
                      ((uint16_t)b[RAW5_FIRST + (2U * i) + 1U] << 8));
}

static bool looks_raw5(const uint8_t *b)
{
    for (uint32_t i = 0U; i < CAPSENSE_SEGMENTS; i++)
    {
        uint16_t r = seg_raw(b, i);
        if ((r < RAW5_MIN) || (r > RAW5_MAX))
        {
            return false;
        }
    }
    return true;
}

static void raw5_set_baseline(const uint8_t *b)
{
    for (uint32_t i = 0U; i < CAPSENSE_SEGMENTS; i++)
    {
        bsl_q4[i] = (int32_t)seg_raw(b, i) << BSL_SHIFT;
    }
    touched    = false;
    rise_reads = 0U;
}

bsp_status_t capsense_rebaseline(void)
{
    uint8_t b[RAW5_BYTES];
    bsp_status_t s = read_bytes(b, (CAPSENSE_FMT_RAW5 == fmt) ? RAW5_BYTES : 3U);
    if (BSP_OK != s)
    {
        return s;
    }
    if (CAPSENSE_FMT_RAW5 == fmt)
    {
        raw5_set_baseline(b);
    }
    else
    {
        idle0 = decode_button(b[0]);
        idle1 = decode_button(b[1]);
    }
    return BSP_OK;
}

bsp_status_t capsense_init(void)
{
    uint8_t b[RAW5_BYTES];
    fmt = CAPSENSE_FMT_NONE;
    if (!i2c5_is_ready())
    {
        return BSP_ERR_INIT;
    }
    bsp_status_t s = BSP_ERR_NODEV;
    for (uint32_t i = 0U; (BSP_OK != s) && (i < CAPSENSE_INIT_TRIES); i++)
    {
        if (i > 0U)
        {
            board_delay_ms(20U);    /* seen 2026-10-10: NODEV right after an E84 reset */
        }
        s = read_bytes(b, RAW5_BYTES);
    }
    if (BSP_OK != s)
    {
        return s;
    }
    fmt = looks_raw5(b) ? CAPSENSE_FMT_RAW5 : CAPSENSE_FMT_FRAME3;
    slider_last = 0U;
    return capsense_rebaseline();
}

static void raw5_eval(const uint8_t *b, capsense_state_t *st)
{
    int32_t maxdrop = 0;
    int32_t wsum = 0, psum = 0;
    bool    quiet = true, all_above = true;
    for (uint32_t i = 0U; i < CAPSENSE_SEGMENTS; i++)
    {
        int32_t d = (bsl_q4[i] >> BSL_SHIFT) - (int32_t)seg_raw(b, i);
        st->drop[i] = (int16_t)d;
        maxdrop = (d > maxdrop) ? d : maxdrop;
        quiet     = quiet && (d < RAW5_NOISE) && (d > -RAW5_NOISE);
        all_above = all_above && (d <= -RAW5_NOISE);
        if (d > RAW5_WEIGHT_FLOOR)
        {
            wsum += d - RAW5_WEIGHT_FLOOR;
            psum += (int32_t)i * (d - RAW5_WEIGHT_FLOOR);
        }
    }
    touched = touched ? (maxdrop > CAPSENSE_TOUCH_OFF) : (maxdrop > CAPSENSE_TOUCH_ON);
    if (touched && (wsum > 0))
    {
        slider_last = (uint8_t)(((psum * 100) + (wsum * (int32_t)(CAPSENSE_SEGMENTS - 1U) / 2)) /
                                (wsum * (int32_t)(CAPSENSE_SEGMENTS - 1U)));
    }
    /* baseline follows slow drift only while nothing is touched; a long rise on
     * every segment (environment change) re-baselines */
    rise_reads = all_above ? (rise_reads + 1U) : 0U;
    for (uint32_t i = 0U; quiet && (i < CAPSENSE_SEGMENTS); i++)
    {
        bsl_q4[i] += (((int32_t)seg_raw(b, i) << BSL_SHIFT) - bsl_q4[i]) >> BSL_SHIFT;
    }
    if (rise_reads >= RAW5_RISE_READS)
    {
        raw5_set_baseline(b);
    }
    st->slider_touched = touched;
    st->slider         = slider_last;
}

bsp_status_t capsense_read(capsense_state_t *st)
{
    uint8_t b[RAW5_BYTES];
    if (NULL == st)
    {
        return BSP_ERR_PARAM;
    }
    if (CAPSENSE_FMT_NONE == fmt)
    {
        return BSP_ERR_INIT;
    }
    (void)memset(st, 0, sizeof(*st));
    bsp_status_t s = read_bytes(b, (CAPSENSE_FMT_RAW5 == fmt) ? RAW5_BYTES : 3U);
    if (BSP_OK != s)
    {
        return s;
    }
    if (CAPSENSE_FMT_RAW5 == fmt)
    {
        raw5_eval(b, st);
        return BSP_OK;
    }
    st->has_buttons    = true;
    st->btn0           = (decode_button(b[0]) != idle0);
    st->btn1           = (decode_button(b[1]) != idle1);
    st->slider         = b[2];
    st->slider_touched = (0U != b[2]);
    (void)memcpy(st->raw, b, 3U);
    return BSP_OK;
}

uint8_t capsense_sdk_code(uint8_t raw)
{
    return decode_button(raw);
}

capsense_fmt_t capsense_format(void)
{
    return fmt;
}

const char *capsense_format_name(void)
{
    static const char *const names[] = { "none", "frame3", "raw5" };
    return names[fmt];
}

uint32_t capsense_last_rslt(void)
{
    return last_rslt;
}

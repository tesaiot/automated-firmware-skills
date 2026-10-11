/*******************************************************************************
* File Name : qwa309_led_matrix.c             component: qwa309-led-matrix
*
* Layer 1 - BSP (QWA309). DFR0522 16x8 RGB matrix @0x10 on SCB5. Protocol, the
* 5-byte command and the shadow-buffer diff follow the TESAIoT SDK guideline
* (bento-firmware-template-mtb-only/proj_cm55/modules/dfr0522_rgb/dfr0522_rgb.c -
* read, rewritten for CM33_NS + i2c-scb5, never built here). The SDK drives the
* panel from CM55 (owner of SCB5 there) with LVGL timers; this component has no
* timers and no IPC - the app owns SCB5 on one core.
*******************************************************************************/
#include "qwa309_led_matrix.h"

#include <string.h>
#include "i2c_scb5.h"
#include "qwa309.h"            /* compile-time board-scope gate + I2C address */

#define REG_FUNC        (0x02U)
#define FUNC_CLEAR      (0x01U)
#define FUNC_PIXEL      (0x08U)
#define FUNC_FILL       (0x09U)

static uint8_t  shadow[LEDM_PIXELS];
static bool     shadow_valid;
static uint32_t last_rslt;

static bsp_status_t cmd(uint8_t func, uint8_t color, uint8_t x, uint8_t y)
{
    const uint8_t tx[5] = { REG_FUNC, func, color, x, y };
    bsp_status_t s = i2c5_write(QWA309_I2C_ADDR_LED_MATRIX, tx, sizeof(tx));
    last_rslt = i2c5_last_rslt();
    return s;
}

bsp_status_t ledm_clear(void)
{
    bsp_status_t s = cmd(FUNC_CLEAR, LEDM_OFF, 0U, 0U);
    if (BSP_OK == s)
    {
        (void)memset(shadow, LEDM_OFF, sizeof(shadow));
        shadow_valid = true;
    }
    return s;
}

bsp_status_t ledm_init(void)
{
    if (!i2c5_is_ready())
    {
        return BSP_ERR_INIT;
    }
    shadow_valid = false;
    return ledm_clear();
}

bsp_status_t ledm_fill(uint8_t color)
{
    if ((color < LEDM_RED) || (color > LEDM_WHITE))
    {
        return BSP_ERR_PARAM;
    }
    bsp_status_t s = cmd(FUNC_FILL, color, 0U, 0U);
    if (BSP_OK == s)
    {
        (void)memset(shadow, color, sizeof(shadow));
        shadow_valid = true;
    }
    return s;
}

bsp_status_t ledm_pixel(uint8_t x, uint8_t y, uint8_t color)
{
    if ((x >= LEDM_WIDTH) || (y >= LEDM_HEIGHT) || (color > LEDM_WHITE))
    {
        return BSP_ERR_PARAM;
    }
    bsp_status_t s = cmd(FUNC_PIXEL, color, x, y);
    if (BSP_OK == s)
    {
        shadow[((uint32_t)y * LEDM_WIDTH) + x] = color;
    }
    return s;
}

bsp_status_t ledm_frame(const uint8_t frame[LEDM_PIXELS], uint32_t *sent)
{
    uint32_t n = 0U;
    bsp_status_t s = BSP_OK;
    if (NULL == frame)
    {
        return BSP_ERR_PARAM;
    }
    if (!shadow_valid)
    {
        s = ledm_clear();               /* unknown panel state -> known blank */
    }
    for (uint32_t i = 0U; (BSP_OK == s) && (i < LEDM_PIXELS); i++)
    {
        uint8_t c = (frame[i] > LEDM_WHITE) ? LEDM_OFF : frame[i];
        if (c != shadow[i])
        {
            s = cmd(FUNC_PIXEL, c, (uint8_t)(i % LEDM_WIDTH), (uint8_t)(i / LEDM_WIDTH));
            if (BSP_OK == s)
            {
                shadow[i] = c;          /* shadow stays true up to a failure */
                n++;
            }
        }
    }
    if (NULL != sent)
    {
        *sent = n;
    }
    return s;
}

/* 3x5 font (SDK guideline): one byte per row, bits 2..0 = left..right */
static const uint8_t font_digit[10][5] = {
    {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
    {7,4,7,1,7}, {7,4,7,5,7}, {7,1,2,2,2}, {7,5,7,5,7}, {7,5,7,1,7},
};
static const uint8_t font_upper[26][5] = {
    {7,5,7,5,5}, {6,5,6,5,6}, {7,4,4,4,7}, {6,5,5,5,6}, {7,4,7,4,7},
    {7,4,7,4,4}, {7,4,5,5,7}, {5,5,7,5,5}, {7,2,2,2,7}, {1,1,1,5,7},
    {5,6,4,6,5}, {4,4,4,4,7}, {5,7,7,5,5}, {6,5,5,5,5}, {7,5,5,5,7},
    {7,5,7,4,4}, {7,5,5,7,3}, {6,5,6,5,5}, {7,4,7,1,7}, {7,2,2,2,2},
    {5,5,5,5,7}, {5,5,5,5,2}, {5,5,7,7,5}, {5,5,2,5,5}, {5,5,7,2,2},
    {7,1,2,4,7},
};

static const uint8_t *glyph(char ch)
{
    if ((ch >= '0') && (ch <= '9'))
    {
        return font_digit[ch - '0'];
    }
    if ((ch >= 'a') && (ch <= 'z'))
    {
        ch = (char)(ch - 'a' + 'A');    /* lower case drawn as upper case */
    }
    if ((ch >= 'A') && (ch <= 'Z'))
    {
        return font_upper[ch - 'A'];
    }
    return NULL;                        /* space / unsupported = blank */
}

/* one glyph column (0..2) at panel column px, rows y = 1..5 */
static void glyph_col(uint8_t *frame, int32_t px, char ch, uint32_t col, uint8_t color)
{
    const uint8_t *rows = glyph(ch);
    if ((NULL == rows) || (px < 0) || (px >= (int32_t)LEDM_WIDTH))
    {
        return;
    }
    for (uint32_t r = 0U; r < 5U; r++)
    {
        if (0U != (rows[r] & (uint8_t)(1U << (2U - col))))
        {
            frame[((r + 1U) * LEDM_WIDTH) + (uint32_t)px] = color;
        }
    }
}

void ledm_digits(uint8_t frame[LEDM_PIXELS], uint16_t value, uint8_t color)
{
    int32_t px = (int32_t)LEDM_WIDTH - 2;  /* rightmost digit ends at column 14 */
    if (value > 9999U)
    {
        value = 9999U;
    }
    (void)memset(frame, LEDM_OFF, LEDM_PIXELS);
    do
    {
        char d = (char)('0' + (value % 10U));
        for (uint32_t col = 0U; col < 3U; col++)
        {
            glyph_col(frame, px - 2 + (int32_t)col, d, col, color);
        }
        px -= 4;
        value /= 10U;
    } while (value > 0U);
}

void ledm_bar(uint8_t frame[LEDM_PIXELS], uint32_t value, uint32_t max, uint8_t color)
{
    uint32_t lit = 0U;
    if (max > 0U)
    {
        lit = (((value > max) ? max : value) * LEDM_WIDTH) / max;
    }
    for (uint32_t x = 0U; x < LEDM_WIDTH; x++)
    {
        frame[((LEDM_HEIGHT - 1U) * LEDM_WIDTH) + x] = (x < lit) ? color : (uint8_t)LEDM_OFF;
    }
}

bool ledm_text(uint8_t frame[LEDM_PIXELS], const char *text, uint32_t offset, uint8_t color)
{
    uint32_t len = (NULL != text) ? (uint32_t)strlen(text) : 0U;
    uint32_t total = len * 4U;            /* 3 glyph columns + 1 gap per char */
    (void)memset(frame, LEDM_OFF, LEDM_PIXELS);
    for (uint32_t c = 0U; c < total; c++)
    {
        int32_t px = (int32_t)LEDM_WIDTH + (int32_t)c - (int32_t)offset;
        if ((c % 4U) < 3U)
        {
            glyph_col(frame, px, text[c / 4U], c % 4U, color);
        }
    }
    return offset <= (total + LEDM_WIDTH);
}

uint32_t ledm_last_rslt(void)
{
    return last_rslt;
}

/*******************************************************************************
* File Name : touch.c                         component: touch-ft5406
*
* Layer 1 - BSP (CM55). See touch.h. Guidelines: Infineon lvgl-demo and
* E84_TouchUI lv_port_indev.c (read, rewritten): DOWN or CONTACT = pressed,
* x/y mirrored against the 800x480 panel.
*******************************************************************************/
#include "touch.h"

#include <string.h>
#include "cybsp.h"
#include "mtb_ctp_ft5406.h"
#include "display.h"
#include "i2c_scb5.h"

static mtb_ctp_ft5406_config_t ctp;
static bool     ready;
static bool     was_pressed;
static uint32_t press_count;
static uint32_t error_count;
static uint32_t last_rslt;

bsp_status_t touch_init(void)
{
    if (!i2c5_is_ready())
    {
        return BSP_ERR_INIT;
    }
    (void)memset(&ctp, 0, sizeof(ctp));
    ctp.i2c_base    = (CySCB_Type *)i2c5_scb_base();
    ctp.i2c_context = (cy_stc_scb_i2c_context_t *)i2c5_scb_context();
    cy_en_scb_i2c_status_t r = mtb_ctp_ft5406_init(&ctp);
    if (CY_SCB_I2C_SUCCESS != r)
    {
        last_rslt = (uint32_t)r;
        return BSP_ERR_NODEV;
    }
    ready = true;
    return BSP_OK;
}

static uint16_t mirror(int v, uint32_t size)
{
    if (v < 0)
    {
        v = 0;
    }
    if ((uint32_t)v >= size)
    {
        v = (int)size - 1;
    }
    return (uint16_t)(size - 1U - (uint32_t)v);
}

bsp_status_t touch_read(bool *pressed, uint16_t *x, uint16_t *y)
{
    if ((NULL == pressed) || (NULL == x) || (NULL == y))
    {
        return BSP_ERR_PARAM;
    }
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    mtb_ctp_touch_event_t ev = MTB_CTP_TOUCH_RESERVED;
    int tx = 0, ty = 0;
    cy_en_scb_i2c_status_t r = mtb_ctp_ft5406_get_single_touch(&ev, &tx, &ty);
    if (CY_SCB_I2C_SUCCESS != r)
    {
        last_rslt = (uint32_t)r;
        error_count++;
        return BSP_ERR_BUS;
    }
    *pressed = (MTB_CTP_TOUCH_DOWN == ev) || (MTB_CTP_TOUCH_CONTACT == ev);
    if (*pressed)
    {
        *x = mirror(tx, DISPLAY_WIDTH);
        *y = mirror(ty, DISPLAY_HEIGHT);
        if (!was_pressed)
        {
            press_count++;
        }
    }
    was_pressed = *pressed;
    return BSP_OK;
}

uint32_t touch_press_count(void) { return press_count; }
uint32_t touch_error_count(void) { return error_count; }
uint32_t touch_last_rslt(void)   { return last_rslt; }

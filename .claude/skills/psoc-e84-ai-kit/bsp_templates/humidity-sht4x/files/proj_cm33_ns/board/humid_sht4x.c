/*******************************************************************************
* File Name : humid_sht4x.c                   component: humidity-sht4x
*
* Layer 1 - BSP. SHT40 @0x44 on SCB0 through Infineon sensor-humidity-sht4x
* (Sensirion driver: temperature in milli-degC, humidity in milli-%RH).
*******************************************************************************/
#include "humid_sht4x.h"

#include <stdbool.h>
#include "i2c_scb0.h"
#include "mtb_sht4x.h"

static bool     ready;
static uint32_t last_rslt;

static bsp_status_t fail(cy_rslt_t r)
{
    last_rslt = (uint32_t)r;
    return i2c0_probe(I2C0_ADDR_SHT40) ? BSP_ERR_BUS : BSP_ERR_NODEV;
}

bsp_status_t humid_init(void)
{
    mtb_hal_i2c_t *h = (mtb_hal_i2c_t *)i2c0_hal();
    if (NULL == h)
    {
        return BSP_ERR_INIT;
    }
    i2c0_lock();
    cy_rslt_t r = mtb_sht4x_init(h, MTB_SHT40_ADDRESS_DEFAULT);
    i2c0_unlock();
    if (CY_RSLT_SUCCESS != r)
    {
        ready = false;
        return fail(r);
    }
    ready = true;
    return BSP_OK;
}

void humid_deinit(void)
{
    mtb_hal_i2c_t *h = (mtb_hal_i2c_t *)i2c0_hal();
    if (ready && (NULL != h))
    {
        mtb_sht4x_free(h);
    }
    ready = false;
}

bsp_status_t humid_read(humid_precision_t prec, int32_t *temp_mc, int32_t *rh_mpct)
{
    mtb_hal_i2c_t *h = (mtb_hal_i2c_t *)i2c0_hal();
    int32_t t = 0, rh = 0;
    cy_rslt_t r;
    if (!ready || (NULL == h))
    {
        return BSP_ERR_INIT;
    }
    i2c0_lock();
    switch (prec)
    {
        case HUMID_PREC_HIGH:   r = mtb_sht4x_measure_high_precision(h, &t, &rh);   break;
        case HUMID_PREC_MEDIUM: r = mtb_sht4x_measure_medium_precision(h, &t, &rh); break;
        case HUMID_PREC_LOW:    r = mtb_sht4x_measure_low_precision(h, &t, &rh);    break;
        default:                r = 1U;                                             break;
    }
    i2c0_unlock();
    if (prec > HUMID_PREC_LOW)
    {
        return BSP_ERR_PARAM;
    }
    if (CY_RSLT_SUCCESS != r)
    {
        return fail(r);
    }
    if (rh < 0)                 /* the formula can leave 0..100 % slightly */
    {
        rh = 0;
    }
    else if (rh > 100000)
    {
        rh = 100000;
    }
    if (NULL != temp_mc)
    {
        *temp_mc = t;
    }
    if (NULL != rh_mpct)
    {
        *rh_mpct = rh;
    }
    return BSP_OK;
}

bsp_status_t humid_soft_reset(void)
{
    mtb_hal_i2c_t *h = (mtb_hal_i2c_t *)i2c0_hal();
    if (NULL == h)
    {
        return BSP_ERR_INIT;
    }
    i2c0_lock();
    cy_rslt_t r = mtb_sht4x_soft_reset(h);
    i2c0_unlock();
    return (CY_RSLT_SUCCESS == r) ? BSP_OK : fail(r);
}

uint32_t humid_last_rslt(void)
{
    return last_rslt;
}

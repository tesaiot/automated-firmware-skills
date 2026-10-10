/*******************************************************************************
* File Name : baro_dps368.c                   component: pressure-dps368
*
* Layer 1 - BSP. DPS368 @0x77 on SCB0 through Infineon sensor-xensiv-dps3xx v2.
* xensiv_dps3xx_read() returns pressure in hPa and temperature in degC
* (float); this layer converts to integer Pa and m°C.
*******************************************************************************/
#include "baro_dps368.h"

#include "i2c_scb0.h"
#include "mtb_xensiv_dps3xx.h"

static xensiv_dps3xx_t dev;
static bool            ready;
static uint32_t        last_rslt;

static bsp_status_t fail(cy_rslt_t r)
{
    last_rslt = (uint32_t)r;
    return i2c0_probe(I2C0_ADDR_DPS368) ? BSP_ERR_BUS : BSP_ERR_NODEV;
}

/* 1,2,4..128 -> log2 (DPS368 rate/oversample register codes); -1 if not a power of two */
static int log2_of(uint8_t v)
{
    for (int i = 0; i < 8; i++)
    {
        if ((uint8_t)(1U << i) == v)
        {
            return i;
        }
    }
    return -1;
}

static int32_t round_to_i32(float v)
{
    return (int32_t)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
}

bsp_status_t baro_init(const baro_config_t *cfg)
{
    mtb_hal_i2c_t *h = (mtb_hal_i2c_t *)i2c0_hal();
    if (NULL == h)
    {
        return BSP_ERR_INIT;
    }
    int rate = 0, osr = 0;
    if (NULL != cfg)
    {
        rate = log2_of(cfg->rate_hz);
        osr  = log2_of(cfg->oversample);
        if ((rate < 0) || (osr < 0))
        {
            return BSP_ERR_PARAM;
        }
    }
    i2c0_lock();
    cy_rslt_t r = mtb_xensiv_dps3xx_init_i2c(&dev, h, XENSIV_DPS3XX_I2C_ADDR_DEFAULT);
    if ((CY_RSLT_SUCCESS == r) && (NULL != cfg))
    {
        xensiv_dps3xx_config_t c;
        r = xensiv_dps3xx_get_config(&dev, &c);
        if (CY_RSLT_SUCCESS == r)
        {
            c.pressure_rate          = (xensiv_dps3xx_rate_t)(rate << 4);
            c.temperature_rate       = (xensiv_dps3xx_rate_t)(rate << 4);
            c.pressure_oversample    = (xensiv_dps3xx_oversample_t)osr;
            c.temperature_oversample = (xensiv_dps3xx_oversample_t)osr;
            r = xensiv_dps3xx_set_config(&dev, &c);
        }
    }
    i2c0_unlock();
    if (CY_RSLT_SUCCESS != r)
    {
        ready = false;
        return fail(r);
    }
    ready = true;
    return BSP_OK;
}

void baro_deinit(void)
{
    if (ready)
    {
        xensiv_dps3xx_free(&dev);
    }
    ready = false;
}

bsp_status_t baro_read(int32_t *pressure_pa, int32_t *temp_mc)
{
    static bool    have_last;
    static int32_t last_pa, last_mc;
    cy_float32_t p = 0.0f, t = 0.0f;
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    i2c0_lock();
    cy_rslt_t r = xensiv_dps3xx_read(&dev, &p, &t);
    i2c0_unlock();
    bsp_status_t s = BSP_OK;
    if (XENSIV_DPS3XX_RSLT_ERR_DATA_NOT_READY == r)
    {
        /* polled faster than the measurement rate (the library's data-ready wait
         * does not actually wait: its loop bound is "timeout && retry") */
        last_rslt = (uint32_t)r;
        if (!have_last)
        {
            return BSP_ERR_NODATA;
        }
        s = BSP_ERR_NODATA;
    }
    else if (CY_RSLT_SUCCESS != r)
    {
        return fail(r);
    }
    else
    {
        last_pa   = round_to_i32(p * 100.0f);      /* hPa -> Pa */
        last_mc   = round_to_i32(t * 1000.0f);
        have_last = true;
    }
    if (NULL != pressure_pa)
    {
        *pressure_pa = last_pa;
    }
    if (NULL != temp_mc)
    {
        *temp_mc = last_mc;
    }
    return s;
}

bool baro_ready(void)
{
    bool p = false, t = false;
    if (!ready)
    {
        return false;
    }
    i2c0_lock();
    cy_rslt_t r = xensiv_dps3xx_check_ready(&dev, &p, &t);
    i2c0_unlock();
    return (CY_RSLT_SUCCESS == r) && p && t;
}

bsp_status_t baro_revision(uint8_t *rev_id)
{
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    if (NULL == rev_id)
    {
        return BSP_ERR_PARAM;
    }
    i2c0_lock();
    cy_rslt_t r = xensiv_dps3xx_get_revision_id(&dev, rev_id);
    i2c0_unlock();
    return (CY_RSLT_SUCCESS == r) ? BSP_OK : fail(r);
}

uint32_t baro_last_rslt(void)
{
    return last_rslt;
}

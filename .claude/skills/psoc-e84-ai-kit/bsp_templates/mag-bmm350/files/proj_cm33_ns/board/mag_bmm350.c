/*******************************************************************************
* File Name : mag_bmm350.c                    component: mag-bmm350
*
* Layer 1 - BSP. BMM350 on the I3C controller (CYBSP_I3C_CONTROLLER, P3.0/P3.1)
* through Infineon sensor-orientation-bmm350 COMPONENT_BMM350_I3C, following the
* library README quick start: PDL Cy_I3C_Init + IRQ + Cy_I3C_Enable, then
* mtb_bmm350_init_i3c() with static address 0x15 (MTB_BMM350_ADDRESS_SEC).
* The library returns field in uT and temperature in degC (float).
*******************************************************************************/
#include "mag_bmm350.h"

#include "board.h"
#include "cybsp.h"
#include "mtb_bmm350.h"

#define MAG_I3C_IRQ_PRIORITY    (2UL)

static cy_stc_i3c_context_t i3c_ctx;
/* static on purpose: mtb_bmm350_init_i3c() keeps a pointer to it and every later
 * transfer uses its dynamicAddress (a stack copy breaks all reads after init) */
static cy_stc_i3c_device_t  i3c_dev;
static mtb_bmm350_t         dev;
static bool                 i3c_up;
static bool                 ready;
static uint32_t             last_rslt;

static void mag_i3c_isr(void)
{
    Cy_I3C_Interrupt(CYBSP_I3C_CONTROLLER_HW, &i3c_ctx);
}

static int32_t round_to_i32(float v)
{
    return (int32_t)((v >= 0.0f) ? (v + 0.5f) : (v - 0.5f));
}

static bsp_status_t i3c_start(void)
{
    if (i3c_up)
    {
        return BSP_OK;
    }
    const cy_stc_sysint_t irq_cfg =
    {
        .intrSrc      = CYBSP_I3C_CONTROLLER_IRQ,
        .intrPriority = MAG_I3C_IRQ_PRIORITY,
    };
    cy_en_i3c_status_t st = Cy_I3C_Init(CYBSP_I3C_CONTROLLER_HW, &CYBSP_I3C_CONTROLLER_config,
                                        &i3c_ctx);
    if (CY_I3C_SUCCESS != st)
    {
        last_rslt = (uint32_t)st;
        return BSP_ERR_INIT;
    }
    if (CY_SYSINT_SUCCESS != Cy_SysInt_Init(&irq_cfg, &mag_i3c_isr))
    {
        last_rslt = 2U;
        return BSP_ERR_INIT;
    }
    NVIC_EnableIRQ((IRQn_Type)irq_cfg.intrSrc);
    Cy_I3C_Enable(CYBSP_I3C_CONTROLLER_HW, &i3c_ctx);
    i3c_up = true;
    return BSP_OK;
}

bsp_status_t mag_init(void)
{
    /* 0x15 = AI Kit default (ADSEL high); 0x14 if the strap/rework differs. Trying both
     * follows E84_TouchUI app_sensor/bmm350/bmm350_driver.c (WheelSense project). */
    static const uint8_t addrs[] = { MTB_BMM350_ADDRESS_SEC, MTB_BMM350_ADDRESS_DEFAULT };
    bsp_status_t s = i3c_start();
    if (BSP_OK != s)
    {
        return s;
    }
    ready = false;
    cy_rslt_t r = CY_RSLT_TYPE_ERROR;
    for (uint32_t i = 0U; (i < (sizeof(addrs) / sizeof(addrs[0]))) && (CY_RSLT_SUCCESS != r); i++)
    {
        i3c_dev = (cy_stc_i3c_device_t){ .staticAddress = addrs[i] };
        r = mtb_bmm350_init_i3c(&dev, CYBSP_I3C_CONTROLLER_HW, &i3c_ctx, &i3c_dev);
    }
    if (CY_RSLT_SUCCESS != r)
    {
        last_rslt = (uint32_t)r;
        return BSP_ERR_NODEV;
    }
    ready = true;           /* library default: normal mode, 25 Hz, averaging x8 */
    return BSP_OK;
}

void mag_deinit(void)
{
    if (i3c_up)
    {
        NVIC_DisableIRQ((IRQn_Type)CYBSP_I3C_CONTROLLER_IRQ);
        Cy_I3C_Disable(CYBSP_I3C_CONTROLLER_HW, &i3c_ctx);
        Cy_I3C_DeInit(CYBSP_I3C_CONTROLLER_HW, &i3c_ctx);
        i3c_up = false;
    }
    ready = false;
}

bsp_status_t mag_set_rate(mag_odr_t odr, mag_perf_t perf)
{
    static const enum bmm350_data_rates rates[] =
    {
        BMM350_DATA_RATE_12_5HZ, BMM350_DATA_RATE_25HZ, BMM350_DATA_RATE_50HZ,
        BMM350_DATA_RATE_100HZ, BMM350_DATA_RATE_200HZ, BMM350_DATA_RATE_400HZ
    };
    static const enum bmm350_performance_parameters perfs[] =
    {
        BMM350_LOWPOWER, BMM350_REGULARPOWER, BMM350_LOWNOISE, BMM350_ULTRALOWNOISE
    };
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    if (((unsigned)odr >= (sizeof(rates) / sizeof(rates[0]))) ||
        ((unsigned)perf >= (sizeof(perfs) / sizeof(perfs[0]))))
    {
        return BSP_ERR_PARAM;
    }
    cy_rslt_t r = mtb_bmm350_set_odr_performance(rates[odr], perfs[perf], &dev);
    if (CY_RSLT_SUCCESS != r)
    {
        last_rslt = (uint32_t)r;
        return BSP_ERR_BUS;
    }
    return BSP_OK;
}

bsp_status_t mag_read(mag_sample_t *s)
{
    mtb_bmm350_data_t d;
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    if (NULL == s)
    {
        return BSP_ERR_PARAM;
    }
    cy_rslt_t r = mtb_bmm350_read(&dev, &d);
    if (CY_RSLT_SUCCESS != r)
    {
        last_rslt = (uint32_t)r;
        return BSP_ERR_BUS;
    }
    s->x_nt    = round_to_i32(d.sensor_data.x * 1000.0f);      /* uT -> nT */
    s->y_nt    = round_to_i32(d.sensor_data.y * 1000.0f);
    s->z_nt    = round_to_i32(d.sensor_data.z * 1000.0f);
    s->temp_mc = round_to_i32(d.sensor_data.temperature * 1000.0f);
    s->t_ms    = board_millis();
    return BSP_OK;
}

bsp_status_t mag_selftest(void)
{
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    cy_rslt_t r = mtb_bmm350_selftest(&dev);
    if (CY_RSLT_SUCCESS != r)
    {
        last_rslt = (uint32_t)r;
        return BSP_ERR_NODEV;
    }
    return BSP_OK;
}

uint32_t mag_last_rslt(void)
{
    return last_rslt;
}

/*******************************************************************************
* File Name : i2c_scb0.c                      component: i2c-scb0
*
* Layer 1 - BSP. SCB0 sensor I2C (1.8 V, P8.0 SCL / P8.1 SDA) as a polled
* controller: PDL init from the BSP configurator (CYBSP_I2C_CONTROLLER_config)
* + mtb_hal_i2c on top, as mtb-example-psoc-edge-imu-bmi270-freertos does.
* The HAL controller transfers poll the SCB (no ISR needed), bounded by
* I2C0_TIMEOUT_MS per byte phase.
*******************************************************************************/
#include "i2c_scb0.h"

#include "cybsp.h"
#include "mtb_hal.h"

static mtb_hal_i2c_t             hal_obj;
static cy_stc_scb_i2c_context_t  i2c_ctx;
static bool                      ready;
static uint32_t                  bus_hz = I2C0_DEFAULT_HZ;
static uint32_t                  last_rslt;
static void                    (*lock_fn)(void);
static void                    (*unlock_fn)(void);

static bsp_status_t to_status(cy_rslt_t r)
{
    last_rslt = (uint32_t)r;
    if (CY_RSLT_SUCCESS == r)
    {
        return BSP_OK;
    }
    switch ((uint32_t)r)
    {
        case (uint32_t)CY_SCB_I2C_MASTER_MANUAL_TIMEOUT:  return BSP_ERR_TIMEOUT;
        case (uint32_t)CY_SCB_I2C_MASTER_MANUAL_ADDR_NAK: return BSP_ERR_NODEV;
        default:                                          return BSP_ERR_BUS;
    }
}

void i2c0_set_lock(void (*lock)(void), void (*unlock)(void))
{
    lock_fn   = lock;
    unlock_fn = unlock;
}

void i2c0_lock(void)
{
    if (NULL != lock_fn)
    {
        lock_fn();
    }
}

void i2c0_unlock(void)
{
    if (NULL != unlock_fn)
    {
        unlock_fn();
    }
}

bsp_status_t i2c0_init(uint32_t hz)
{
    if ((100000UL != hz) && (400000UL != hz))
    {
        return BSP_ERR_PARAM;
    }
    if (ready)
    {
        i2c0_deinit();
    }
    if (CY_SCB_I2C_SUCCESS != Cy_SCB_I2C_Init(CYBSP_I2C_CONTROLLER_HW,
                                              &CYBSP_I2C_CONTROLLER_config, &i2c_ctx))
    {
        last_rslt = 1U;
        return BSP_ERR_INIT;
    }
    Cy_SCB_I2C_Enable(CYBSP_I2C_CONTROLLER_HW);

    cy_rslt_t r = mtb_hal_i2c_setup(&hal_obj, &CYBSP_I2C_CONTROLLER_hal_config, &i2c_ctx, NULL);
    if (CY_RSLT_SUCCESS == r)
    {
        const mtb_hal_i2c_cfg_t cfg =
        {
            .is_target               = false,
            .address                 = 0U,
            .frequency_hz            = hz,
            .address_mask            = MTB_HAL_I2C_DEFAULT_ADDR_MASK,
            .enable_address_callback = false,
        };
        r = mtb_hal_i2c_configure(&hal_obj, &cfg);
    }
    if (CY_RSLT_SUCCESS != r)
    {
        last_rslt = (uint32_t)r;
        Cy_SCB_I2C_Disable(CYBSP_I2C_CONTROLLER_HW, &i2c_ctx);
        Cy_SCB_I2C_DeInit(CYBSP_I2C_CONTROLLER_HW);
        return BSP_ERR_INIT;
    }
    bus_hz = hz;
    ready  = true;
    return BSP_OK;
}

void i2c0_deinit(void)
{
    if (ready)
    {
        Cy_SCB_I2C_Disable(CYBSP_I2C_CONTROLLER_HW, &i2c_ctx);
        Cy_SCB_I2C_DeInit(CYBSP_I2C_CONTROLLER_HW);
        ready = false;
    }
}

bool i2c0_is_ready(void)
{
    return ready;
}

bsp_status_t i2c0_write(uint8_t addr, const uint8_t *data, size_t len)
{
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    if ((len > 0xFFFFU) || ((NULL == data) && (0U != len)))
    {
        return BSP_ERR_PARAM;
    }
    i2c0_lock();
    bsp_status_t s = to_status(mtb_hal_i2c_controller_write(&hal_obj, addr, data, (uint16_t)len,
                                                            I2C0_TIMEOUT_MS, true));
    i2c0_unlock();
    return s;
}

bsp_status_t i2c0_read(uint8_t addr, uint8_t *data, size_t len)
{
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    if ((NULL == data) || (0U == len) || (len > 0xFFFFU))
    {
        return BSP_ERR_PARAM;
    }
    i2c0_lock();
    bsp_status_t s = to_status(mtb_hal_i2c_controller_read(&hal_obj, addr, data, (uint16_t)len,
                                                           I2C0_TIMEOUT_MS, true));
    i2c0_unlock();
    return s;
}

bsp_status_t i2c0_write_read(uint8_t addr, const uint8_t *wr, size_t wr_len,
                             uint8_t *rd, size_t rd_len)
{
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    if ((NULL == wr) || (0U == wr_len) || (wr_len > 0xFFFFU) ||
        (NULL == rd) || (0U == rd_len) || (rd_len > 0xFFFFU))
    {
        return BSP_ERR_PARAM;
    }
    i2c0_lock();
    bsp_status_t s = to_status(mtb_hal_i2c_controller_write(&hal_obj, addr, wr, (uint16_t)wr_len,
                                                            I2C0_TIMEOUT_MS, false));
    if (BSP_OK == s)
    {
        s = to_status(mtb_hal_i2c_controller_read(&hal_obj, addr, rd, (uint16_t)rd_len,
                                                  I2C0_TIMEOUT_MS, true));
    }
    else
    {
        (void)Cy_SCB_I2C_MasterSendStop(CYBSP_I2C_CONTROLLER_HW, I2C0_TIMEOUT_MS, &i2c_ctx);
    }
    i2c0_unlock();
    return s;
}

bsp_status_t i2c0_reg_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    return i2c0_write_read(addr, &reg, 1U, data, len);
}

bsp_status_t i2c0_reg_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len)
{
    uint8_t buf[33];
    if ((len > (sizeof(buf) - 1U)) || ((NULL == data) && (0U != len)))
    {
        return BSP_ERR_PARAM;
    }
    buf[0] = reg;
    for (size_t i = 0U; i < len; i++)
    {
        buf[i + 1U] = data[i];
    }
    return i2c0_write(addr, buf, len + 1U);
}

bool i2c0_probe(uint8_t addr)
{
    return BSP_OK == i2c0_write(addr, NULL, 0U);   /* START + address + STOP */
}

uint32_t i2c0_scan(uint8_t *found, uint32_t max)
{
    uint32_t n = 0U;
    for (uint8_t a = 0x08U; a <= 0x77U; a++)
    {
        if (i2c0_probe(a))
        {
            if ((NULL != found) && (n < max))
            {
                found[n] = a;
            }
            n++;
        }
    }
    return n;
}

/* 9 SCL pulses + STOP on plain GPIO frees a target that holds SDA low after an
 * interrupted transfer; then the pins go back to the BSP configuration. */
bsp_status_t i2c0_recover(void)
{
    i2c0_deinit();
    Cy_GPIO_SetHSIOM(CYBSP_I2C_SCL_PORT, CYBSP_I2C_SCL_PIN, HSIOM_SEL_GPIO);
    Cy_GPIO_SetHSIOM(CYBSP_I2C_SDA_PORT, CYBSP_I2C_SDA_PIN, HSIOM_SEL_GPIO);
    Cy_GPIO_SetDrivemode(CYBSP_I2C_SCL_PORT, CYBSP_I2C_SCL_PIN, CY_GPIO_DM_OD_DRIVESLOW);
    Cy_GPIO_SetDrivemode(CYBSP_I2C_SDA_PORT, CYBSP_I2C_SDA_PIN, CY_GPIO_DM_OD_DRIVESLOW);
    Cy_GPIO_Write(CYBSP_I2C_SDA_PORT, CYBSP_I2C_SDA_PIN, 1U);
    for (uint32_t i = 0U; i < 9U; i++)
    {
        Cy_GPIO_Write(CYBSP_I2C_SCL_PORT, CYBSP_I2C_SCL_PIN, 0U);
        Cy_SysLib_DelayUs(5U);
        Cy_GPIO_Write(CYBSP_I2C_SCL_PORT, CYBSP_I2C_SCL_PIN, 1U);
        Cy_SysLib_DelayUs(5U);
    }
    Cy_GPIO_Write(CYBSP_I2C_SDA_PORT, CYBSP_I2C_SDA_PIN, 0U);    /* STOP: SDA low -> high */
    Cy_SysLib_DelayUs(5U);
    Cy_GPIO_Write(CYBSP_I2C_SDA_PORT, CYBSP_I2C_SDA_PIN, 1U);
    Cy_SysLib_DelayUs(5U);
    bool sda_free = (0U != Cy_GPIO_Read(CYBSP_I2C_SDA_PORT, CYBSP_I2C_SDA_PIN));
    (void)Cy_GPIO_Pin_Init(CYBSP_I2C_SCL_PORT, CYBSP_I2C_SCL_PIN, &CYBSP_I2C_SCL_config);
    (void)Cy_GPIO_Pin_Init(CYBSP_I2C_SDA_PORT, CYBSP_I2C_SDA_PIN, &CYBSP_I2C_SDA_config);
    bsp_status_t s = i2c0_init(bus_hz);
    return (BSP_OK != s) ? s : (sda_free ? BSP_OK : BSP_ERR_BUS);
}

uint32_t i2c0_last_rslt(void)
{
    return last_rslt;
}

void *i2c0_hal(void)
{
    return ready ? (void *)&hal_obj : NULL;
}

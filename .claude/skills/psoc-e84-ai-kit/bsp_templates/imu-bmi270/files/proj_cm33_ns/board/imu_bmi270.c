/*******************************************************************************
* File Name : imu_bmi270.c                    component: imu-bmi270
*
* Layer 1 - BSP. BMI270 @0x68 on SCB0 through Infineon sensor-motion-bmi270
* (Bosch BMI270 SensorAPI). mtb_bmi270_config_default() enables accel + gyro
* (100 Hz, +-2 g, +-2000 dps); imu_init() then applies the requested ranges/ODRs.
* Raw counts are scaled with the *configured* range: value = raw * range / 32768.
*******************************************************************************/
#include "imu_bmi270.h"

#include "board.h"
#include "i2c_scb0.h"
#include "mtb_bmi270.h"

static mtb_bmi270_t dev;
static imu_config_t cur = IMU_CONFIG_DEFAULT;
static bool         ready;
static uint32_t     last_rslt;

static bsp_status_t fail(cy_rslt_t r)
{
    last_rslt = (uint32_t)r;
    return i2c0_probe(I2C0_ADDR_BMI270) ? BSP_ERR_BUS : BSP_ERR_NODEV;
}

static bool acc_range_code(uint8_t g, uint8_t *code)
{
    switch (g)
    {
        case 2U:  *code = BMI2_ACC_RANGE_2G;  return true;
        case 4U:  *code = BMI2_ACC_RANGE_4G;  return true;
        case 8U:  *code = BMI2_ACC_RANGE_8G;  return true;
        case 16U: *code = BMI2_ACC_RANGE_16G; return true;
        default:  return false;
    }
}

static bool acc_odr_code(uint16_t hz, uint8_t *code)
{
    switch (hz)
    {
        case 25U:   *code = BMI2_ACC_ODR_25HZ;   return true;
        case 50U:   *code = BMI2_ACC_ODR_50HZ;   return true;
        case 100U:  *code = BMI2_ACC_ODR_100HZ;  return true;
        case 200U:  *code = BMI2_ACC_ODR_200HZ;  return true;
        case 400U:  *code = BMI2_ACC_ODR_400HZ;  return true;
        case 800U:  *code = BMI2_ACC_ODR_800HZ;  return true;
        case 1600U: *code = BMI2_ACC_ODR_1600HZ; return true;
        default:    return false;
    }
}

static bool gyr_range_code(uint16_t dps, uint8_t *code)
{
    switch (dps)
    {
        case 125U:  *code = BMI2_GYR_RANGE_125;  return true;
        case 250U:  *code = BMI2_GYR_RANGE_250;  return true;
        case 500U:  *code = BMI2_GYR_RANGE_500;  return true;
        case 1000U: *code = BMI2_GYR_RANGE_1000; return true;
        case 2000U: *code = BMI2_GYR_RANGE_2000; return true;
        default:    return false;
    }
}

static bool gyr_odr_code(uint16_t hz, uint8_t *code)
{
    switch (hz)
    {
        case 25U:   *code = BMI2_GYR_ODR_25HZ;   return true;
        case 50U:   *code = BMI2_GYR_ODR_50HZ;   return true;
        case 100U:  *code = BMI2_GYR_ODR_100HZ;  return true;
        case 200U:  *code = BMI2_GYR_ODR_200HZ;  return true;
        case 400U:  *code = BMI2_GYR_ODR_400HZ;  return true;
        case 800U:  *code = BMI2_GYR_ODR_800HZ;  return true;
        case 1600U: *code = BMI2_GYR_ODR_1600HZ; return true;
        case 3200U: *code = BMI2_GYR_ODR_3200HZ; return true;
        default:    return false;
    }
}

bsp_status_t imu_init(const imu_config_t *cfg)
{
    const imu_config_t want = (NULL != cfg) ? *cfg : (imu_config_t)IMU_CONFIG_DEFAULT;
    uint8_t ar, ao, gr, go;
    mtb_hal_i2c_t *h = (mtb_hal_i2c_t *)i2c0_hal();
    if (NULL == h)
    {
        return BSP_ERR_INIT;
    }
    if (!acc_range_code(want.acc_range_g, &ar) || !acc_odr_code(want.acc_odr_hz, &ao) ||
        !gyr_range_code(want.gyr_range_dps, &gr) || !gyr_odr_code(want.gyr_odr_hz, &go))
    {
        return BSP_ERR_PARAM;
    }
    ready = false;
    i2c0_lock();
    cy_rslt_t r = mtb_bmi270_init_i2c(&dev, h, MTB_BMI270_ADDRESS_DEFAULT);
    if (CY_RSLT_SUCCESS == r)
    {
        r = mtb_bmi270_config_default(&dev);
    }
    if (CY_RSLT_SUCCESS == r)
    {
        mtb_bmi270_sens_config_t c[2];
        c[0].sensor_config.type = BMI2_ACCEL;
        c[1].sensor_config.type = BMI2_GYRO;
        r = mtb_bmi270_get_sensor_config(c, 2U, &dev);
        if (CY_RSLT_SUCCESS == r)
        {
            c[0].sensor_config.cfg.acc.range = ar;
            c[0].sensor_config.cfg.acc.odr   = ao;
            c[1].sensor_config.cfg.gyr.range = gr;
            c[1].sensor_config.cfg.gyr.odr   = go;
            r = mtb_bmi270_set_sensor_config(c, 2U, &dev);
        }
    }
    i2c0_unlock();
    if (CY_RSLT_SUCCESS != r)
    {
        return fail(r);
    }
    cur   = want;
    ready = true;
    return BSP_OK;
}

void imu_deinit(void)
{
    if (ready)
    {
        const uint8_t list[2] = { BMI2_ACCEL, BMI2_GYRO };
        i2c0_lock();
        (void)mtb_bmi270_sensor_disable(list, 2U, &dev);
        i2c0_unlock();
    }
    ready = false;
}

bsp_status_t imu_get_config(imu_config_t *cfg)
{
    if (NULL == cfg)
    {
        return BSP_ERR_PARAM;
    }
    *cfg = cur;
    return ready ? BSP_OK : BSP_ERR_INIT;
}

static int32_t scale(int16_t raw, uint32_t range_milli)
{
    return (int32_t)(((int64_t)raw * (int64_t)range_milli) / 32768);
}

bsp_status_t imu_read(imu_sample_t *s)
{
    mtb_bmi270_data_t d;
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    if (NULL == s)
    {
        return BSP_ERR_PARAM;
    }
    i2c0_lock();
    cy_rslt_t r = mtb_bmi270_read(&dev, &d);
    i2c0_unlock();
    if (CY_RSLT_SUCCESS != r)
    {
        return fail(r);
    }
    const uint32_t acc = (uint32_t)cur.acc_range_g * 1000U;      /* mg   full scale */
    const uint32_t gyr = (uint32_t)cur.gyr_range_dps * 1000U;    /* mdps full scale */
    s->ax_mg   = scale(d.sensor_data.acc.x, acc);
    s->ay_mg   = scale(d.sensor_data.acc.y, acc);
    s->az_mg   = scale(d.sensor_data.acc.z, acc);
    s->gx_mdps = scale(d.sensor_data.gyr.x, gyr);
    s->gy_mdps = scale(d.sensor_data.gyr.y, gyr);
    s->gz_mdps = scale(d.sensor_data.gyr.z, gyr);
    s->t_ms    = board_millis();
    return BSP_OK;
}

bsp_status_t imu_read_temp(int32_t *temp_mc)
{
    uint16_t raw = 0U;
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    if (NULL == temp_mc)
    {
        return BSP_ERR_PARAM;
    }
    i2c0_lock();
    cy_rslt_t r = mtb_bmi270_read_temp(&dev, &raw);
    i2c0_unlock();
    if (CY_RSLT_SUCCESS != r)
    {
        return fail(r);
    }
    if (0x8000U == raw)                      /* "invalid" marker of the BMI270 */
    {
        return BSP_ERR_NODATA;
    }
    /* two's complement, 0 = 23 degC, 1/512 K per LSB */
    *temp_mc = 23000 + (int32_t)(((int32_t)(int16_t)raw * 1000) / 512);
    return BSP_OK;
}

bsp_status_t imu_selftest(void)
{
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    i2c0_lock();
    cy_rslt_t r = mtb_bmi270_selftest(&dev);
    i2c0_unlock();
    /* the self-test soft-resets the part: restore the configuration either way */
    bsp_status_t again = imu_init(&cur);
    if (CY_RSLT_SUCCESS != r)
    {
        last_rslt = (uint32_t)r;
        return BSP_ERR_NODEV;
    }
    return again;
}

bsp_status_t imu_set_data_ready_cb(void (*cb)(void *arg), void *arg)
{
    (void)cb;
    (void)arg;
    return BSP_ERR_PARAM;                    /* INT1 routing: not implemented yet */
}

bool imu_data_ready(void)
{
    uint8_t status = 0U;
    if (!ready)
    {
        return false;
    }
    i2c0_lock();
    int8_t r = bmi2_get_status(&status, &dev.sensor);
    i2c0_unlock();
    return (BMI2_OK == r) && (0U != (status & BMI2_DRDY_ACC));
}

uint32_t imu_last_rslt(void)
{
    return last_rslt;
}

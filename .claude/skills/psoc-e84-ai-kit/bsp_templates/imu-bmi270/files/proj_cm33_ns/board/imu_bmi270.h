/*******************************************************************************
* File Name : imu_bmi270.h                    component: imu-bmi270 (planned)
*
* Layer 1 - BSP. Bosch BMI270 6-axis IMU (U18) of the PSOC Edge E84 AI Kit.
*   I2C SCB0 @0x68 (i2c_scb0.h), INT1 P7.7, INT2 P6.6, 1.8 V.
* Backend: Infineon sensor-motion-bmi270 (mtb_bmi270_init_i2c / _config_default /
*   _set_sensor_config / _read / _read_temp / _selftest) on i2c0_hal().
* Units are integers so func/ and UART tests need no float printf:
*   acceleration mg, angular rate mdps (milli-degree/s), temperature m°C.
* Scaling follows the configured range (LSB/g = 32768 / range_g) - never a
*   fixed constant (TESAIoT SDK bug noted in the guideline).
*******************************************************************************/
#ifndef IMU_BMI270_H
#define IMU_BMI270_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

typedef struct
{
    uint8_t  acc_range_g;     /* 2, 4, 8, 16                       default 4    */
    uint16_t acc_odr_hz;      /* 25, 50, 100, 200, 400, 800, 1600  default 100  */
    uint16_t gyr_range_dps;   /* 125, 250, 500, 1000, 2000         default 2000 */
    uint16_t gyr_odr_hz;      /* 25 .. 3200                        default 100  */
} imu_config_t;

#define IMU_CONFIG_DEFAULT  { 4U, 100U, 2000U, 100U }

typedef struct
{
    int32_t  ax_mg, ay_mg, az_mg;
    int32_t  gx_mdps, gy_mdps, gz_mdps;
    uint32_t t_ms;            /* board_millis() at read time */
} imu_sample_t;

/* Needs i2c0_init() first. cfg NULL = IMU_CONFIG_DEFAULT. Checks chip id. */
bsp_status_t imu_init(const imu_config_t *cfg);
void         imu_deinit(void);
bsp_status_t imu_get_config(imu_config_t *cfg);

bsp_status_t imu_read(imu_sample_t *s);           /* latest sample           */
bsp_status_t imu_read_temp(int32_t *temp_mc);     /* die temperature, m°C    */
bsp_status_t imu_selftest(void);                  /* Bosch self-test, ~200 ms */

/* Data-ready on INT1 (P7.7, falling edge). cb runs in IRQ context: set a flag
 * only. Pass NULL to disable. NOT IMPLEMENTED YET: returns BSP_ERR_PARAM.
 * imu_data_ready() reads the accel data-ready bit of the STATUS register. */
bsp_status_t imu_set_data_ready_cb(void (*cb)(void *arg), void *arg);
bool         imu_data_ready(void);

uint32_t     imu_last_rslt(void);

#endif /* IMU_BMI270_H */

/*******************************************************************************
* File Name : mag_bmm350.h                    component: mag-bmm350 (planned)
*
* Layer 1 - BSP. Bosch BMM350 3-axis magnetometer (U20) of the PSOC Edge E84
*   AI Kit. **I3C** (I3C_CORE, P3.0 SCL / P3.1 SDA, CYBSP_I3C_CONTROLLER),
*   static address 0x15 (init falls back to 0x14), MAG_INT P6.4, 1.8 V. (I2C needs rework R220/R223.)
* Backend: Infineon sensor-orientation-bmm350 COMPONENT_BMM350_I3C
*   (mtb_bmm350_init_i3c / _read / _set_odr_performance / _selftest).
*   Build: COMPONENTS+=BMM350_I3C and the component patch of bmm350.c (Bosch
*   SensorAPI soft-reset drops the part back to I2C - library README).
*   I3C open-drain rate is kept low (datasheet: SCL high >= 160 ns).
* Units: field nT (1 uT = 1000 nT), temperature m°C, integers.
* This component owns the I3C controller (claims I3C).
*******************************************************************************/
#ifndef MAG_BMM350_H
#define MAG_BMM350_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

typedef struct
{
    int32_t  x_nt, y_nt, z_nt;
    int32_t  temp_mc;
    uint32_t t_ms;
} mag_sample_t;

typedef enum
{
    MAG_ODR_12HZ5 = 0, MAG_ODR_25HZ, MAG_ODR_50HZ, MAG_ODR_100HZ, MAG_ODR_200HZ, MAG_ODR_400HZ
} mag_odr_t;

typedef enum
{
    MAG_PERF_LOW_POWER = 0,   /* averaging off  */
    MAG_PERF_REGULAR,
    MAG_PERF_LOW_NOISE,
    MAG_PERF_ULTRA_LOW_NOISE  /* averaging x8   */
} mag_perf_t;

/* Inits the I3C controller + IRQ, assigns the dynamic address, starts normal
 * mode at the library default (25 Hz, averaging x8 = MAG_PERF_ULTRA_LOW_NOISE).
 * Re-run after a sensor reset (dynamic address assignment again). */
bsp_status_t mag_init(void);
void         mag_deinit(void);
bsp_status_t mag_set_rate(mag_odr_t odr, mag_perf_t perf);

bsp_status_t mag_read(mag_sample_t *s);
bsp_status_t mag_selftest(void);

uint32_t     mag_last_rslt(void);

#endif /* MAG_BMM350_H */

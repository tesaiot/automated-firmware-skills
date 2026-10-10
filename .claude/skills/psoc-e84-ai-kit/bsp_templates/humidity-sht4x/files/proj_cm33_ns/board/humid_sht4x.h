/*******************************************************************************
* File Name : humid_sht4x.h                   component: humidity-sht4x (planned)
*
* Layer 1 - BSP. Sensirion SHT40 humidity + temperature (U22) of the PSOC Edge
*   E84 AI Kit. I2C SCB0 @0x44, 1.8 V, no interrupt pin.
* Backend: Infineon sensor-humidity-sht4x (mtb_sht4x_init / _measure_*_precision
*   / _soft_reset) on i2c0_hal(). Measurement is blocking: high ~9 ms,
*   medium ~5 ms, low ~2 ms (CRC-8 0x31 checked by the library).
* Units: temperature m°C, relative humidity m%RH (0..100000), integers.
*******************************************************************************/
#ifndef HUMID_SHT4X_H
#define HUMID_SHT4X_H

#include <stdint.h>
#include "bsp_status.h"

typedef enum
{
    HUMID_PREC_HIGH = 0,
    HUMID_PREC_MEDIUM,
    HUMID_PREC_LOW
} humid_precision_t;

/* Needs i2c0_init() first. Soft-resets the sensor and checks it answers. */
bsp_status_t humid_init(void);
void         humid_deinit(void);

bsp_status_t humid_read(humid_precision_t prec, int32_t *temp_mc, int32_t *rh_mpct);
bsp_status_t humid_soft_reset(void);

uint32_t     humid_last_rslt(void);

#endif /* HUMID_SHT4X_H */

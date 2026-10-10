/*******************************************************************************
* File Name : baro_dps368.h                   component: pressure-dps368 (planned)
*
* Layer 1 - BSP. Infineon XENSIV DPS368 barometric pressure + temperature (U6)
*   of the PSOC Edge E84 AI Kit. I2C SCB0 @0x77 (0x76 with R84), INT P11.3.
* Backend: Infineon sensor-xensiv-dps3xx v2 (mtb_xensiv_dps3xx_init_i2c +
*   xensiv_dps3xx_set_config / _read / _check_ready) on i2c0_hal().
* Units: pressure Pa, temperature m°C (integers).
*******************************************************************************/
#ifndef BARO_DPS368_H
#define BARO_DPS368_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

typedef struct
{
    uint8_t rate_hz;          /* 1, 2, 4, 8, 16, 32, 64, 128   default 8  */
    uint8_t oversample;       /* 1, 2, 4, 8, 16, 32, 64, 128   default 16 */
} baro_config_t;

#define BARO_CONFIG_DEFAULT  { 8U, 16U }

/* Needs i2c0_init() first. cfg NULL = the library's default configuration
 * (background mode); a cfg sets rate + oversampling of both pressure and temperature. */
bsp_status_t baro_init(const baro_config_t *cfg);
void         baro_deinit(void);

/* Latest sample (NULL ok). BSP_ERR_NODATA = no new sample since the last read
 * (polled faster than the rate); the outputs then hold the previous sample. */
bsp_status_t baro_read(int32_t *pressure_pa, int32_t *temp_mc);
bool         baro_ready(void);               /* new pressure+temp available */
bsp_status_t baro_revision(uint8_t *rev_id);  /* product/revision register   */

uint32_t     baro_last_rslt(void);

#endif /* BARO_DPS368_H */

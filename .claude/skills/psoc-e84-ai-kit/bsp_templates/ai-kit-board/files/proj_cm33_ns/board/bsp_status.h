/*******************************************************************************
* File Name : bsp_status.h
*
* Layer 1 - BSP adaptation (board layer). Shared return codes of every bsp
* driver API in this pack (i2c_scb0.h, imu_bmi270.h, ...). func/ and main.c
* test against BSP_OK and print bsp_status_str() - never a Cy_/mtb_ result.
* The driver keeps the raw vendor result for diagnostics (<drv>_last_rslt()).
*******************************************************************************/
#ifndef BSP_STATUS_H
#define BSP_STATUS_H

typedef enum
{
    BSP_OK = 0,
    BSP_ERR_PARAM,      /* bad argument / unsupported setting               */
    BSP_ERR_INIT,       /* not initialised, or init of the peripheral failed */
    BSP_ERR_BUS,        /* I2C/I3C/SPI transfer error (NACK, arbitration)    */
    BSP_ERR_NODEV,      /* device did not answer / wrong chip id            */
    BSP_ERR_TIMEOUT,    /* bounded wait expired                             */
    BSP_ERR_BUSY,       /* resource held by another user / core             */
    BSP_ERR_NODATA,     /* no new sample / frame yet (non-blocking read)     */
    BSP_ERR_OVERFLOW,   /* FIFO / ring buffer overrun, data lost            */
    BSP_ERR_HW_SETTING  /* board setting missing (connector, jumper, rework) */
} bsp_status_t;

static inline const char *bsp_status_str(bsp_status_t s)
{
    static const char *const names[] = {
        "OK", "ERR_PARAM", "ERR_INIT", "ERR_BUS", "ERR_NODEV", "ERR_TIMEOUT",
        "ERR_BUSY", "ERR_NODATA", "ERR_OVERFLOW", "ERR_HW_SETTING"
    };
    return ((unsigned)s < (sizeof(names) / sizeof(names[0]))) ? names[s] : "ERR_?";
}

#endif /* BSP_STATUS_H */

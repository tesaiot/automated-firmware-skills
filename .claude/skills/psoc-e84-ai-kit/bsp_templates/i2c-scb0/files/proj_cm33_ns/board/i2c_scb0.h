/*******************************************************************************
* File Name : i2c_scb0.h                      component: i2c-scb0 (planned)
*
* Layer 1 - BSP. Sensor I2C bus of the PSOC Edge E84 AI Kit, 1.8 V:
*   SCB0, P8.0 SCL / P8.1 SDA (CYBSP_I2C_CONTROLLER = CYBSP_I2C_*_1V8).
* Devices on this bus (UG 002-41506 Rev *B):
*   0x18 TLV320DAC3100 audio DAC   0x44 SHT40   0x68 BMI270 (0x69 rework R85/R86)
*   0x77 DPS368 (0x76 rework R84)
* The KitProg3 USB-I2C bridge is hard-wired to the same pins: do not use the
* bridge while the firmware drives the bus.
*
* One owner: this component (claims SCB0). Device drivers (imu_bmi270.c, ...)
* call it; func/ code never talks to the bus directly.
* Implementation notes: Cy_SCB_I2C_Init(CYBSP_I2C_CONTROLLER_HW,
* &CYBSP_I2C_CONTROLLER_config) + mtb_hal_i2c_setup(&CYBSP_I2C_CONTROLLER_hal_config)
* as in mtb-example-psoc-edge-imu-bmi270-freertos; bounded waits; bus recovery
* = 9 SCL pulses + STOP on GPIO, then re-init (disable the SCB first).
*******************************************************************************/
#ifndef I2C_SCB0_H
#define I2C_SCB0_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bsp_status.h"

#define I2C0_ADDR_TLV320DAC3100   (0x18U)
#define I2C0_ADDR_SHT40           (0x44U)
#define I2C0_ADDR_BMI270          (0x68U)
#define I2C0_ADDR_DPS368          (0x77U)

#define I2C0_DEFAULT_HZ           (400000UL)
#define I2C0_TIMEOUT_MS           (50U)

/* Init the controller (100000 or 400000 Hz). Safe to call again (re-init). */
bsp_status_t i2c0_init(uint32_t hz);
void         i2c0_deinit(void);
bool         i2c0_is_ready(void);

/* Raw transfers, 7-bit address, blocking with I2C0_TIMEOUT_MS bound. */
bsp_status_t i2c0_write(uint8_t addr, const uint8_t *data, size_t len);
bsp_status_t i2c0_read(uint8_t addr, uint8_t *data, size_t len);
bsp_status_t i2c0_write_read(uint8_t addr, const uint8_t *wr, size_t wr_len,
                             uint8_t *rd, size_t rd_len);   /* repeated START */

/* 8-bit register helpers (most sensors on this bus). */
bsp_status_t i2c0_reg_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
bsp_status_t i2c0_reg_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);

/* Diagnostics */
bool         i2c0_probe(uint8_t addr);                      /* ACK on address  */
uint32_t     i2c0_scan(uint8_t *found, uint32_t max);       /* 0x08..0x77      */
bsp_status_t i2c0_recover(void);                            /* unstick SDA     */
uint32_t     i2c0_last_rslt(void);                          /* raw PDL/HAL code */

/* Exec-model hook: an RTOS registers a mutex here (exec component), so device
 * drivers stay exec-agnostic. Baremetal: leave unset (no locking). */
void         i2c0_set_lock(void (*lock)(void), void (*unlock)(void));
/* Device drivers wrap each vendor-library call in lock/unlock (no-op when unset). */
void         i2c0_lock(void);
void         i2c0_unlock(void);

/* For Infineon mtb_* device libraries only (bsp layer): returns the
 * mtb_hal_i2c_t* set up on SCB0 (as void* so this header stays HAL-free). */
void        *i2c0_hal(void);

#endif /* I2C_SCB0_H */

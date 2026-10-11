/*******************************************************************************
* File Name : i2c_scb5.h      component: i2c-scb5 (CM33_NS) and i2c-scb5-cm55 (same file)
*
* Layer 1 - BSP. 3.3 V I2C of the PSOC Edge E84 AI Kit:
*   SCB5, P17.0 SCL / P17.1 SDA (CYBSP_I2C_CAM_CONTROLLER = CYBSP_I2C_DISPLAY_CONTROLLER,
*   pins CYBSP_I2C_*_3V3, port 17 = VDDIO6 3.3 V).
* Wired to: MIPI-DSI connector J10 (display panel MCU, e.g. Waveshare 4.3" @0x45),
*   J16 (touch, 4-pin, not fitted: FT5406 @0x38), QWIIC J7 (not fitted),
*   DVP camera header J14 (kits before Rev *A only).
* On TESAIoT the QWA309 base board hangs its I2C parts here too (qwa309 pack).
*
* One owner core: this component claims SCB5 on CM33_NS. display-dsi (CM55)
* also claims SCB5 -> the gate blocks the pair unless exec scb5-lock is used.
* Same API shape as i2c_scb0.h.
*******************************************************************************/
#ifndef I2C_SCB5_H
#define I2C_SCB5_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bsp_status.h"

#define I2C5_ADDR_FT5406          (0x38U)   /* touch of Waveshare DSI panels */
#define I2C5_ADDR_WS_PANEL_MCU    (0x45U)   /* Waveshare 4.3" backlight/power */

/* 100 kHz: conservative for the whole shared bus. Read from live SCB5 registers
 * 2026-10-10: an older TESAIoT SDK build ran ~100 kHz, release fw-mpy v2.4.8 runs
 * ~450 kHz (vendor BSP divider) - CapSense behaved the same with either, so speed is
 * not critical. 400000 is still accepted by i2c5_init(). */
#define I2C5_DEFAULT_HZ           (100000UL)
#define I2C5_TIMEOUT_MS           (50U)

bsp_status_t i2c5_init(uint32_t hz);
void         i2c5_deinit(void);
bool         i2c5_is_ready(void);

bsp_status_t i2c5_write(uint8_t addr, const uint8_t *data, size_t len);
bsp_status_t i2c5_read(uint8_t addr, uint8_t *data, size_t len);
bsp_status_t i2c5_write_read(uint8_t addr, const uint8_t *wr, size_t wr_len,
                             uint8_t *rd, size_t rd_len);
bsp_status_t i2c5_reg_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
bsp_status_t i2c5_reg_write(uint8_t addr, uint8_t reg, const uint8_t *data, size_t len);

bool         i2c5_probe(uint8_t addr);
uint32_t     i2c5_scan(uint8_t *found, uint32_t max);
bsp_status_t i2c5_recover(void);
/* Diagnostics: current SCL/SDA levels (1 = released/high). Both must read 1 on an idle bus;
 * a 0 means a device (or a missing pull-up / switch) holds the line low. */
void         i2c5_lines(bool *scl, bool *sda);
/* Diagnostics: levels with the MCU internal pull-ups on (bus briefly taken over, then
 * re-initialised). 1/1 here but 0/0 in i2c5_lines() = no external pull-ups; 0 = held low. */
bsp_status_t i2c5_pullup_probe(bool *scl, bool *sda);
uint32_t     i2c5_last_rslt(void);

void         i2c5_set_lock(void (*lock)(void), void (*unlock)(void));
/* Device drivers wrap each transfer sequence in lock/unlock (no-op when unset). */
void         i2c5_lock(void);
void         i2c5_unlock(void);

/* For Infineon mtb_* device libraries only (bsp layer): the mtb_hal_i2c_t* set up on SCB5. */
void        *i2c5_hal(void);

/* Interrupt-driven PDL transfers (Cy_SCB_I2C_MasterWrite/Read - used by Infineon's
 * display-dsi-* / touch-ctp-* libraries) need the SCB5 interrupt: installs a handler
 * that runs Cy_SCB_I2C_Interrupt on this context. Our own i2c5_* calls use the HAL's
 * blocking manual mode and do not depend on it. Call after i2c5_init(). */
bsp_status_t i2c5_irq_enable(uint32_t priority);

/* For PDL-based device libraries (bsp layer only): SCB5 base + I2C context,
 * as void* (CySCB_Type*, cy_stc_scb_i2c_context_t*). */
void        *i2c5_scb_base(void);
void        *i2c5_scb_context(void);

#endif /* I2C_SCB5_H */

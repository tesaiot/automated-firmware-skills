/*******************************************************************************
* File Name : i2c_scb5.h                      component: i2c-scb5 (planned)
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

#define I2C5_DEFAULT_HZ           (400000UL)
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
uint32_t     i2c5_last_rslt(void);

void         i2c5_set_lock(void (*lock)(void), void (*unlock)(void));

/* For PDL-based device libraries (bsp layer only): SCB5 base + I2C context,
 * as void* (CySCB_Type*, cy_stc_scb_i2c_context_t*). */
void        *i2c5_scb_base(void);
void        *i2c5_scb_context(void);

#endif /* I2C_SCB5_H */

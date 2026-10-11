/*******************************************************************************
* File Name : qwa309_pots.h                   component: qwa309-pots
*
* Layer 1 - BSP (QWA309 base board, board id tesaiot only). The four
* potentiometers VR1..VR4 on P15.4..P15.7 = Autonomous Analog SAR GPIO channels
* 4..7, 12-bit, 0..1.8 V (VDDA reference).
*
* Knobs are numbered by their SILKSCREEN label: knob 0 = "VR1" ... knob 3 = "VR4".
* The PCB swaps VR1/VR2 against the schematic: knob "VR1" is P15.5 (ch 5), "VR2" is
* P15.4 (ch 4) - QWA309_POT_CHANNELS in qwa309.h.
*
* Runs on CM33_NS; reading is lock-free (result registers of a free-running SAR),
* so CM55 may read the same registers once pots_init() has run.
* A 0 count does not prove a live channel (unconfigured channels read 0 too):
* use pots_ready().
*******************************************************************************/
#ifndef QWA309_POTS_H
#define QWA309_POTS_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

#define POTS_COUNT          (4U)
#define POTS_MAX_COUNT      (4095U)       /* 12-bit */
#define POTS_FULL_SCALE_MV  (1800U)       /* VDDA */

/* Re-targets the BSP's Autonomous Analog SAR to channels 4..7 (P15.4..P15.7, pins
 * in analog mode), then Cy_AutAnalog_Init + StartAutonomousControl. Call once. */
bsp_status_t pots_init(void);

/* true once all four channels have produced a result since pots_init() */
bool         pots_ready(void);

/* knob = 0..3 for silkscreen VR1..VR4 */
bsp_status_t pots_read_raw(uint32_t knob, uint16_t *count);       /* 0..4095       */
bsp_status_t pots_read_mv(uint32_t knob, uint32_t *mv);            /* 0..1800 mV    */
bsp_status_t pots_read_permille(uint32_t knob, uint32_t *permille);/* 0..1000       */
const char  *pots_name(uint32_t knob);                             /* "VR1".."VR4"  */
const char  *pots_pin(uint32_t knob);                              /* "P15.5" ...   */

uint32_t     pots_last_rslt(void);

#endif /* QWA309_POTS_H */

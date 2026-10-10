/*******************************************************************************
* File Name : psram.h                         component: psram (planned)
*
* Layer 1 - BSP. 128 Mb (16 MB) Infineon S70KS1283 octal HyperRAM / PSRAM (U3)
*   of the PSOC Edge E84 AI Kit: SMIF1 (CYBSP_SMIF_CORE_1_PSRAM), D0-D7
*   P4.0-P4.7, CS P0.1, memory reset P0.0 (shared with the flash reset), RWDS,
*   differential clock, 1.8 V.
* Backend: PDL SMIF + CYBSP_SMIF_CORE_1_PSRAM_config, memory-mapped mode as in
*   mtb-example-psoc-edge-psram-xip. The BSP memory map has no PSRAM region:
*   use psram_base() buffers (frame buffers, ML tensors, audio) - not the linker.
* Runs on CM33_NS before the CM55 boots (both cores may then use the mapping;
*   cache maintenance on CM55 is the user's job).
*******************************************************************************/
#ifndef PSRAM_H
#define PSRAM_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

#define PSRAM_SIZE   (0x01000000UL)   /* 16 MB */

/* Init SMIF1 + device registers, enable memory-mapped access. */
bsp_status_t psram_init(void);
bool         psram_is_ready(void);

void        *psram_base(void);          /* NULL until psram_init() succeeded */
uint32_t     psram_size(void);

/* Destructive pattern test over [offset, offset+len): walking 1s + address. */
bsp_status_t psram_test(uint32_t offset, uint32_t len);

uint32_t     psram_last_rslt(void);

#endif /* PSRAM_H */

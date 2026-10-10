/*******************************************************************************
* File Name : extflash.h                      component: qspi-flash (planned)
*
* Layer 1 - BSP. Data storage in the 512 Mb (64 MB) Infineon S25HS512T QSPI NOR
*   flash (U4) of the PSOC Edge E84 AI Kit: SMIF0, D0-D3 P1.0-P1.3, CS P2.0,
*   1.8 V. The same chip holds the boot chain and all three core images (XIP
*   at 0x60000000; BSP 1.4.0 regions end at offset 0x00880000).
* Window: only EXTFLASH_DATA_OFFSET .. +EXTFLASH_DATA_SIZE is reachable through
*   this API; offsets below are relative to the window. The app chooses the area
*   (components.py option flash_area -> DEFINES): fs-10mb = 0x00A00000 + 4 MB
*   (Infineon littlefs/emFile default), ce-test-sectors = 0x01F80000 + 512 KB,
*   top-16mb = 0x03000000 + 16 MB, or custom (256 KB aligned, >= 0x00880000).
*   Erase unit = 256 KB above 0x40000 (4 KB / 128 KB sectors only at the bottom),
*   program page = 256 B.
* Backend: PDL SMIF + CYBSP_SMIF_CORE_0_XSPI_FLASH_config / smifMemConfigs
*   (cycfg_qspi_memslot), as in mtb-example-psoc-edge-serial-flash-readwrite.
*   Erase/program while executing XIP from the same chip: the routines run from
*   RAM with interrupts masked (or on a core not executing from flash) -
*   follow the example; never erase outside the window.
*******************************************************************************/
#ifndef EXTFLASH_H
#define EXTFLASH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bsp_status.h"

#define EXTFLASH_CHIP_SIZE      (0x04000000UL)   /* 64 MB                    */
/* Compile-time gate: the area comes from the app's Makefile (new_app.sh --opt flash_area=...) */
#if !defined(EXTFLASH_DATA_OFFSET) || !defined(EXTFLASH_DATA_SIZE)
    #error "qspi-flash: no flash area chosen - new_app.sh ... --opt flash_area=<fs-10mb|ce-test-sectors|top-16mb|custom:<off>,<size>>"
#endif
#define EXTFLASH_ERASE_SIZE     (0x00040000UL)   /* 256 KB in the window     */
#define EXTFLASH_PAGE_SIZE      (256UL)

typedef struct
{
    uint32_t jedec_id;          /* manufacturer/device id read at init */
    uint32_t window_offset;
    uint32_t window_size;
    uint32_t erase_size;
    uint32_t page_size;
} extflash_info_t;

bsp_status_t extflash_init(void);
bsp_status_t extflash_info(extflash_info_t *info);

/* Offsets are window-relative. BSP_ERR_PARAM if outside the window or (erase)
 * not aligned to EXTFLASH_ERASE_SIZE. write() needs erased (0xFF) flash. */
bsp_status_t extflash_read(uint32_t offset, void *buf, size_t len);
bsp_status_t extflash_erase(uint32_t offset, size_t len);
bsp_status_t extflash_write(uint32_t offset, const void *buf, size_t len);

/* Memory-mapped (XIP) read pointer for the window, read-only. */
const void  *extflash_xip_ptr(uint32_t offset);

uint32_t     extflash_last_rslt(void);

#endif /* EXTFLASH_H */

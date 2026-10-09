/*******************************************************************************
* File Name : ipc_mailbox.h  (shared by proj_cm33_ns and proj_cm55)
*
* Memory contract between the two application cores - part of the board
* layer: it is the only place that names the shared SOCMEM region.
*
* The mailbox sits in the top 256 bytes of the BSP region m33_m55_shared.
* CM55 maps that region Normal non-cacheable (BSP MPU setup), CM33 has no data
* cache, so plain volatile accesses + __DMB() are enough. CM33 allocates
* .cy_shared_socmem upward from the region start, so the top stays free.
* Proven with the led-uart-ipc demo on E84 (BSP 1.4.0).
*
* Protocol (one writer per field):
*   CM33  init all fields, then magic = IPC_MAGIC, then start CM55
*   CM55  waits for magic, sets cm55_ready = IPC_MAGIC, counts heartbeat
*   CM33  writes cmd + arg, then increments cmd_seq           (request)
*   CM55  executes, writes status + result, then ack_seq = cmd_seq (reply)
* Rules: CM33 never posts a new request before ack_seq == cmd_seq (or a
* timeout); the core that does not own a field only reads it.
*******************************************************************************/
#ifndef IPC_MAILBOX_H
#define IPC_MAILBOX_H

#include <stdint.h>
#include "cybsp.h"

#define IPC_MAGIC           (0x49504331UL)   /* "IPC1" */
#define IPC_RESERVED        (0x100UL)

/* Requests (cmd) */
#define IPC_CMD_PING        (1UL)    /* result = arg + 1                    */
#define IPC_CMD_LED         (2UL)    /* arg = (led << 8) | on; result = readback */

/* Replies (status) */
#define IPC_OK              (0UL)
#define IPC_ERR_CMD         (1UL)    /* unknown cmd                         */
#define IPC_ERR_ARG         (2UL)    /* bad argument (e.g. no such LED)     */
#define IPC_ERR_TIMEOUT     (0xFFUL) /* set by CM33 when no ack arrives     */

typedef struct
{
    volatile uint32_t magic;        /* CM33: IPC_MAGIC once initialized      */
    volatile uint32_t cm55_ready;   /* CM55: IPC_MAGIC once its loop runs    */
    volatile uint32_t heartbeat;    /* CM55: incremented every loop (~10 kHz) */
    volatile uint32_t cmd_seq;      /* CM33: incremented per request         */
    volatile uint32_t cmd;          /* CM33 */
    volatile uint32_t arg;          /* CM33 */
    volatile uint32_t ack_seq;      /* CM55: cmd_seq of the last reply       */
    volatile uint32_t status;       /* CM55 */
    volatile uint32_t result;       /* CM55 */
} ipc_mailbox_t;

#define IPC_MAILBOX ((ipc_mailbox_t *)(CYMEM_m33_m55_shared_START + \
                                       CYMEM_m33_m55_shared_SIZE - IPC_RESERVED))

#endif /* IPC_MAILBOX_H */

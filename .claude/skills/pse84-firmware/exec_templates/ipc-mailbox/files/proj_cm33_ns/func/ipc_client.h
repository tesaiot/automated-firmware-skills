/*******************************************************************************
* File Name : ipc_client.h
*
* Layer 2 - function (CM33 side of the CM33 <-> CM55 mailbox, ../shared).
* Call ipc_client_init() BEFORE board_start_cm55(): CM55 waits for it.
*******************************************************************************/
#ifndef IPC_CLIENT_H
#define IPC_CLIENT_H

#include <stdbool.h>
#include <stdint.h>
#include "ipc_mailbox.h"

#define IPC_CLIENT_TIMEOUT_MS   (100U)

void     ipc_client_init(void);
bool     ipc_client_ready(void);                 /* CM55 loop is running        */
uint32_t ipc_client_heartbeat(void);             /* CM55 loop counter           */
/* Post a request and wait for the reply. Returns IPC_OK, IPC_ERR_* or
 * IPC_ERR_TIMEOUT; *result is valid only for IPC_OK. *rtt_ms = round trip. */
uint32_t ipc_client_call(uint32_t cmd, uint32_t arg, uint32_t *result, uint32_t *rtt_ms);

#endif /* IPC_CLIENT_H */

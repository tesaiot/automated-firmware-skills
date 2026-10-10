/*******************************************************************************
* File Name : ipc_client.c
*
* Layer 2 - function. CM33 requester of the shared mailbox (see
* ../shared/ipc_mailbox.h for the protocol). Polled, no IPC interrupts: the
* BSP's own mtb-ipc/SRF uses IPC channel 0 and MTB_IPC_IRQ_USER..+3.
*******************************************************************************/
#include "ipc_client.h"
#include "board.h"

void ipc_client_init(void)
{
    ipc_mailbox_t *mb = IPC_MAILBOX;

    mb->magic      = 0UL;
    mb->cm55_ready = 0UL;
    mb->heartbeat  = 0UL;
    mb->cmd_seq    = 0UL;
    mb->cmd        = 0UL;
    mb->arg        = 0UL;
    mb->ack_seq    = 0UL;
    mb->status     = IPC_OK;
    mb->result     = 0UL;
    __DMB();
    mb->magic      = IPC_MAGIC;   /* last: CM55 starts trusting the fields now */
    __DMB();
}

bool ipc_client_ready(void)
{
    return IPC_MAGIC == IPC_MAILBOX->cm55_ready;
}

uint32_t ipc_client_heartbeat(void)
{
    return IPC_MAILBOX->heartbeat;
}

uint32_t ipc_client_call(uint32_t cmd, uint32_t arg, uint32_t *result, uint32_t *rtt_ms)
{
    ipc_mailbox_t *mb = IPC_MAILBOX;
    uint32_t seq = mb->cmd_seq + 1UL;
    uint32_t t0  = board_millis();

    mb->cmd = cmd;
    mb->arg = arg;
    __DMB();                      /* cmd/arg visible before the new sequence */
    mb->cmd_seq = seq;

    while (mb->ack_seq != seq)
    {
        if ((board_millis() - t0) > IPC_CLIENT_TIMEOUT_MS)
        {
            return IPC_ERR_TIMEOUT;
        }
    }
    __DMB();                      /* status/result written before ack_seq */
    if (NULL != rtt_ms)
    {
        *rtt_ms = board_millis() - t0;
    }
    if (NULL != result)
    {
        *result = mb->result;
    }
    return mb->status;
}

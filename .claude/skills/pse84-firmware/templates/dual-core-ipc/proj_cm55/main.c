/*******************************************************************************
* File Name : main.c  (CM55)
*
* Layer 3 - application of the worker core. Serves requests that CM33 posts
* in the shared mailbox (../shared/ipc_mailbox.h) and counts a heartbeat so
* CM33 can prove this core is running. No UART here - CM33 owns the console.
* Ported from the led-uart-ipc demo (roles swapped so the skill's console and
* serial tests stay on CM33).
*******************************************************************************/
#include "board55.h"
#include "ipc_mailbox.h"

#define LOOP_DELAY_US   (100U)

static uint32_t handle(uint32_t cmd, uint32_t arg, uint32_t *result)
{
    switch (cmd)
    {
        case IPC_CMD_PING:
            *result = arg + 1UL;
            return IPC_OK;

        case IPC_CMD_LED:
        {
            uint32_t led = arg >> 8;
            if (!board55_led_write(led, 0UL != (arg & 0xFFUL)))
            {
                return IPC_ERR_ARG;
            }
            *result = board55_led_read(led) ? 1UL : 0UL;
            return IPC_OK;
        }

        default:
            return IPC_ERR_CMD;
    }
}

int main(void)
{
    board55_init();

    ipc_mailbox_t *mb = IPC_MAILBOX;

    /* CM33 initializes the mailbox before it enables this core */
    while (IPC_MAGIC != mb->magic)
    {
    }
    uint32_t last_seq = mb->cmd_seq;
    mb->cm55_ready = IPC_MAGIC;

    for (;;)
    {
        uint32_t seq = mb->cmd_seq;

        if (seq != last_seq)
        {
            __DMB();                  /* cmd/arg were written before cmd_seq */
            uint32_t result = 0UL;
            uint32_t status = handle(mb->cmd, mb->arg, &result);

            mb->result = result;
            mb->status = status;
            __DMB();                  /* reply visible before the ack */
            mb->ack_seq = seq;
            last_seq = seq;
        }
        mb->heartbeat++;
        board55_delay_us(LOOP_DELAY_US);
    }
}

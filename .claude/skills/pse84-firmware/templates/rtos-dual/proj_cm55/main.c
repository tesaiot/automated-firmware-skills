/*******************************************************************************
* File Name : main.c  (CM55)
*
* Layer 3 - application, CM55 side of rtos-dual: FreeRTOS on CM55 with one
* "worker" task serving the CM33_NS <-> CM55 mailbox (ipc-mailbox). Polls every
* kernel tick (1 ms) instead of busy-waiting, so other CM55 tasks get the CPU.
* Commands: PING (result = arg + 1), UPTIME (result = CM55 kernel uptime, ms),
* LED (arg = (led << 8) | on). No printf on CM55.
*******************************************************************************/
#include "FreeRTOS.h"
#include "task.h"

#include "board55.h"
#include "ipc_mailbox.h"

#define WORKER_STACK_WORDS  (configMINIMAL_STACK_SIZE * 2U)
#define WORKER_PRIORITY     (tskIDLE_PRIORITY + 2U)

static uint32_t handle(uint32_t cmd, uint32_t arg, uint32_t *result)
{
    switch (cmd)
    {
        case IPC_CMD_PING:
            *result = arg + 1UL;
            return IPC_OK;
        case IPC_CMD_UPTIME:
            *result = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
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

static void worker_task(void *arg)
{
    (void)arg;
    ipc_mailbox_t *mb = IPC_MAILBOX;
    while (IPC_MAGIC != mb->magic)        /* CM33 initializes the mailbox before starting us */
    {
        vTaskDelay(1);
    }
    uint32_t last_seq = mb->cmd_seq;
    mb->cm55_ready = IPC_MAGIC;
    for (;;)
    {
        uint32_t seq = mb->cmd_seq;
        if (seq != last_seq)
        {
            __DMB();                      /* cmd/arg were written before cmd_seq */
            uint32_t result = 0UL;
            uint32_t status = handle(mb->cmd, mb->arg, &result);
            mb->result = result;
            mb->status = status;
            __DMB();                      /* reply visible before the ack */
            mb->ack_seq = seq;
            last_seq = seq;
        }
        mb->heartbeat++;
        vTaskDelay(1);                    /* 1 tick = 1 ms */
    }
}

int main(void)
{
    board55_init();
    if (pdPASS != xTaskCreate(worker_task, "ipc", WORKER_STACK_WORDS, NULL, WORKER_PRIORITY, NULL))
    {
        board55_fatal();
    }
    vTaskStartScheduler();
    board55_fatal();
}

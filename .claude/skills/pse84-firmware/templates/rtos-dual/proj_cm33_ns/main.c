/*******************************************************************************
* File Name : main.c  (CM33 non-secure)
*
* Layer 3 - application. rtos-dual: FreeRTOS on BOTH cores (scope dual,
* cm33_ns=freertos cm55=freertos, IPC template ipc-mailbox).
*   CM33_NS tasks: "console" (UART commands, prio 2) and "blink" (LED1 every 500 ms,
*   prio 1). CM55 runs its own scheduler with a mailbox worker task (proj_cm55).
* Start-up order (as Infineon's FreeRTOS CEs): board init -> mailbox init -> start
* CM55 -> create tasks -> vTaskStartScheduler(). board_millis() counts from the
* scheduler start; ipc_client_call() therefore runs only inside tasks.
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"

#include "board.h"
#include "console.h"
#include "led.h"
#include "ipc_client.h"

#define APP_NAME            "rtos-dual"
#define APP_VERSION         "1.0"
#define CONSOLE_PERIOD_MS   (5U)
#define BLINK_PERIOD_MS     (500U)
#define BLINK_LED           (1U)
#define CONSOLE_STACK_WORDS (configMINIMAL_STACK_SIZE * 4U)
#define BLINK_STACK_WORDS   (configMINIMAL_STACK_SIZE)
#define CONSOLE_PRIORITY    (tskIDLE_PRIORITY + 2U)
#define BLINK_PRIORITY      (tskIDLE_PRIORITY + 1U)

static TaskHandle_t      console_task_h;
static TaskHandle_t      blink_task_h;
static volatile uint32_t blink_count;

static const char *ipc_status(uint32_t st)
{
    switch (st)
    {
        case IPC_OK:          return "ok";
        case IPC_ERR_CMD:     return "cm55 bad cmd";
        case IPC_ERR_ARG:     return "cm55 bad arg";
        case IPC_ERR_TIMEOUT: return "cm55 timeout";
        default:              return "cm55 unknown status";
    }
}

static void print_info(void)
{
    printf("INFO app=%s v=%s board=%s scope=dual cm33_ns=freertos cm55=freertos cm55_ready=%u\r\n",
           APP_NAME, APP_VERSION, BOARD_NAME, ipc_client_ready() ? 1U : 0U);
    printf("READY\r\n");
}

static void cmd_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    print_info();
}

static void cmd_tasks(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    /* high-water marks are in words; print bytes left */
    printf("TASKS n=%lu console_free=%luB blink_free=%luB\r\n",
           (unsigned long)uxTaskGetNumberOfTasks(),
           (unsigned long)(uxTaskGetStackHighWaterMark(console_task_h) * sizeof(StackType_t)),
           (unsigned long)(uxTaskGetStackHighWaterMark(blink_task_h) * sizeof(StackType_t)));
}

static void cmd_blink(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    printf("BLINK count=%lu period_ms=%u led=LED1\r\n", (unsigned long)blink_count,
           (unsigned)BLINK_PERIOD_MS);
}

static void cmd_ping(int argc, char *argv[])
{
    uint32_t arg = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 1U;
    uint32_t result = 0U, rtt = 0U;
    uint32_t st = ipc_client_call(IPC_CMD_PING, arg, &result, &rtt);
    if (IPC_OK == st)
    {
        printf("OK ping arg=%lu result=%lu rtt_ms=%lu\r\n", (unsigned long)arg,
               (unsigned long)result, (unsigned long)rtt);
    }
    else
    {
        printf("ERR ping %s\r\n", ipc_status(st));
    }
}

static void cmd_uptime(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint32_t cm55_ms = 0U, rtt = 0U;
    uint32_t st = ipc_client_call(IPC_CMD_UPTIME, 0U, &cm55_ms, &rtt);
    if (IPC_OK == st)
    {
        printf("UPTIME cm33_ms=%lu cm55_ms=%lu rtt_ms=%lu\r\n", (unsigned long)board_millis(),
               (unsigned long)cm55_ms, (unsigned long)rtt);
    }
    else
    {
        printf("ERR uptime %s\r\n", ipc_status(st));
    }
}

static void cmd_led55(int argc, char *argv[])
{
    if ((argc < 3) || ((0 != strcmp(argv[2], "on")) && (0 != strcmp(argv[2], "off"))))
    {
        printf("ERR usage: led55 <1|2> on|off\r\n");
        return;
    }
    uint32_t led = (uint32_t)strtoul(argv[1], NULL, 10);
    uint32_t on = (0 == strcmp(argv[2], "on")) ? 1U : 0U;
    uint32_t result = 0U, rtt = 0U;
    uint32_t st = ipc_client_call(IPC_CMD_LED, (led << 8) | on, &result, &rtt);
    if (IPC_OK == st)
    {
        printf("OK led55 led=%lu state=%s rtt_ms=%lu\r\n", (unsigned long)led, on ? "on" : "off",
               (unsigned long)rtt);
    }
    else
    {
        printf("ERR led55 %s\r\n", ipc_status(st));
    }
}

static const console_cmd_t commands[] =
{
    { "info",   "info                  - app/board info", cmd_info },
    { "tasks",  "tasks                 - CM33 task count + stack left", cmd_tasks },
    { "blink",  "blink                 - LED1 heartbeat counter", cmd_blink },
    { "ping",   "ping [n]              - CM55 round trip (result n+1)", cmd_ping },
    { "uptime", "uptime                - CM33 and CM55 kernel uptime", cmd_uptime },
    { "led55",  "led55 <1|2> on|off    - LED driven by CM55", cmd_led55 },
};

static void console_task(void *arg)
{
    (void)arg;
    print_info();                         /* scheduler is running: board_millis() ticks */
    for (;;)
    {
        console_poll();
        vTaskDelay(pdMS_TO_TICKS(CONSOLE_PERIOD_MS));
    }
}

static void blink_task(void *arg)
{
    (void)arg;
    TickType_t last = xTaskGetTickCount();
    for (;;)
    {
        (void)led_toggle(BLINK_LED);
        blink_count++;
        vTaskDelayUntil(&last, pdMS_TO_TICKS(BLINK_PERIOD_MS));
    }
}

int main(void)
{
    board_init();
    led_init();
    console_init(commands, sizeof(commands) / sizeof(commands[0]));
    ipc_client_init();                    /* before CM55 starts: it waits for the mailbox */

    printf("\r\n[APP] %s %s on %s (FreeRTOS on CM33_NS + CM55)\r\n", APP_NAME, APP_VERSION,
           BOARD_NAME);
    board_start_cm55();

    if ((pdPASS != xTaskCreate(console_task, "console", CONSOLE_STACK_WORDS, NULL,
                               CONSOLE_PRIORITY, &console_task_h)) ||
        (pdPASS != xTaskCreate(blink_task, "blink", BLINK_STACK_WORDS, NULL,
                               BLINK_PRIORITY, &blink_task_h)))
    {
        printf("FATAL task create failed\r\n");
        board_fatal();
    }
    vTaskStartScheduler();
    board_fatal();                        /* only reached if the kernel could not start */
}

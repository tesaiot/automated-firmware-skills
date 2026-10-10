/*******************************************************************************
* File Name : rtos_hooks.c  (CM55)                   component: freertos-cm55
*
* Execution layer. Kernel hooks enabled in FreeRTOSConfig.h. CM55 has no console:
* stop the core (board55_fatal); CM33_NS notices through the IPC heartbeat / timeout.
*******************************************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "board55.h"

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    (void)name;
    board55_fatal();
}

void vApplicationMallocFailedHook(void)
{
    board55_fatal();
}

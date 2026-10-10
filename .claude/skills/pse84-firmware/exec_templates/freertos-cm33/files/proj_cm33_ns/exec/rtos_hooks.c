/*******************************************************************************
* File Name : rtos_hooks.c  (CM33 non-secure)        component: freertos-cm33
*
* Execution layer. Kernel hooks enabled in FreeRTOSConfig.h
* (configCHECK_FOR_STACK_OVERFLOW 2, configUSE_MALLOC_FAILED_HOOK 1): report on the
* console and stop - a stack overflow or an empty heap is a design error, never a
* runtime condition to recover from.
*******************************************************************************/
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "board.h"

void vApplicationStackOverflowHook(TaskHandle_t task, char *name)
{
    (void)task;
    printf("\r\nFATAL rtos stack overflow task=%s\r\n", (NULL != name) ? name : "?");
    board_console_flush();
    board_fatal();
}

void vApplicationMallocFailedHook(void)
{
    printf("\r\nFATAL rtos heap exhausted (configTOTAL_HEAP_SIZE / newlib heap)\r\n");
    board_console_flush();
    board_fatal();
}

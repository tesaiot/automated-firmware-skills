/*******************************************************************************
* File Name : board.c
*
* Layer 1 - BSP adaptation. Maps the generic board API onto the BSP aliases
* generated from design.modus (CYBSP_USER_LEDx, CYBSP_USER_BTNx,
* CYBSP_DEBUG_UART) plus board extras that are not in the BSP (QWA309 base
* board buttons). BSP pins are configured by cybsp_init(); extras are
* configured here at run time. Missing aliases simply shrink the tables.
*******************************************************************************/
#include "board.h"

#include <stdio.h>

#include "cybsp.h"
#include "retarget_io_init.h"
#if defined(BOARD_HAS_QWA309)
#include "gpio_pse84_bga_220.h"
#include "qwa309.h"
#endif

/* Boot the CM55 image placed after the MCUboot header in m55_nvm */
#define CM55_BOOT_WAIT_TIME_USEC    (10U)
#define CM55_APP_BOOT_ADDR          (CYMEM_CM33_0_m55_nvm_START + CYBSP_MCUBOOT_HEADER_SIZE)

typedef struct
{
    GPIO_PRT_Type *port;
    uint8_t        port_num;
    uint8_t        pin;
    bool           runtime_init;   /* not in the BSP: configure in board_init() */
} board_pin_t;

static const board_pin_t leds[] =
{
#if defined(CYBSP_USER_LED1_PORT)
    { CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PORT_NUM, CYBSP_USER_LED1_PIN, false },
#endif
#if defined(CYBSP_USER_LED2_PORT)
    { CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PORT_NUM, CYBSP_USER_LED2_PIN, false },
#endif
#if defined(CYBSP_USER_LED3_PORT)
    { CYBSP_USER_LED3_PORT, CYBSP_USER_LED3_PORT_NUM, CYBSP_USER_LED3_PIN, false },
#endif
#if defined(CYBSP_USER_LED4_PORT)
    { CYBSP_USER_LED4_PORT, CYBSP_USER_LED4_PORT_NUM, CYBSP_USER_LED4_PIN, false },
#endif
#if defined(CYBSP_USER_LED5_PORT)
    { CYBSP_USER_LED5_PORT, CYBSP_USER_LED5_PORT_NUM, CYBSP_USER_LED5_PIN, false },
#endif
};

static const board_pin_t buttons[] =
{
#if defined(CYBSP_USER_BTN1_PORT) && (BOARD_BSP_BUTTONS >= 1U)
    { CYBSP_USER_BTN1_PORT, CYBSP_USER_BTN1_PORT_NUM, CYBSP_USER_BTN1_PIN, false },
#endif
#if defined(CYBSP_USER_BTN2_PORT) && (BOARD_BSP_BUTTONS >= 2U)
    { CYBSP_USER_BTN2_PORT, CYBSP_USER_BTN2_PORT_NUM, CYBSP_USER_BTN2_PIN, false },
#endif
#if defined(BOARD_HAS_QWA309)
    /* QWA309 SW4 / SW5: active low, need the MCU pull-up. The AI-Kit BSP drives
     * these nets as DVP camera outputs (CYBSP_DVP_CAM_RESET / _PWDN), and P17.5
     * is also the USB-host VBUS enable - buttons, camera and USB host exclude
     * each other. */
    { P17_5_PORT, QWA309_SW4_PORT_NUM, QWA309_SW4_PIN, true },
    { P17_7_PORT, QWA309_SW5_PORT_NUM, QWA309_SW5_PIN, true },
#endif
};

#define LED_COUNT   ((uint32_t)(sizeof(leds) / sizeof(leds[0])))
#define BTN_COUNT   ((uint32_t)(sizeof(buttons) / sizeof(buttons[0])))

#if defined(BOARD_LED_NAMES)
static const char *const led_names[] = { BOARD_LED_NAMES };
#define LED_NAME_COUNT  ((uint32_t)(sizeof(led_names) / sizeof(led_names[0])))
#endif
#if defined(BOARD_BTN_NAMES)
static const char *const btn_names[] = { BOARD_BTN_NAMES };
#define BTN_NAME_COUNT  ((uint32_t)(sizeof(btn_names) / sizeof(btn_names[0])))
#endif

/* 1 ms time base, owned by the CM33_NS execution model:
 * - bare metal (baremetal-cm33): SysTick here; it stops in Deep Sleep, the main loop never sleeps
 * - FreeRTOS (freertos-cm33, COMPONENT_FREERTOS): the kernel owns SysTick (port handler); time
 *   comes from the tick count, which stays 0 until vTaskStartScheduler() */
#if defined(COMPONENT_FREERTOS)
#include "FreeRTOS.h"
#include "task.h"
#else
static volatile uint32_t ms_ticks;

void SysTick_Handler(void)   /* overrides the weak default in the BSP startup */
{
    ms_ticks++;
}
#endif

void board_fatal(void)
{
    handle_app_error();
}

void board_init(void)
{
    if (CY_RSLT_SUCCESS != cybsp_init())
    {
        board_fatal();
    }
    /* Board extras not described in the BSP: input + pull-up (out value 1 holds the pull) */
    for (uint32_t i = 0U; i < BTN_COUNT; i++)
    {
        if (buttons[i].runtime_init)
        {
            Cy_GPIO_Pin_FastInit(buttons[i].port, buttons[i].pin, CY_GPIO_DM_PULLUP,
                                 1UL, HSIOM_SEL_GPIO);
        }
    }
#if !defined(COMPONENT_FREERTOS)
    if (0U != SysTick_Config(SystemCoreClock / 1000U))
    {
        board_fatal();
    }
#endif
    __enable_irq();
    init_retarget_io();   /* SCB2 debug UART + printf */
    for (uint32_t i = 0U; i < LED_COUNT; i++)
    {
        board_led_write(i, false);
    }
}

void board_start_cm55(void)
{
    /* CM55_APP_BOOT_ADDR must be updated if the CM55 memory layout changes. */
    Cy_SysEnableCM55(MXCM55, CM55_APP_BOOT_ADDR, CM55_BOOT_WAIT_TIME_USEC);
}

void board_delay_ms(uint32_t ms)
{
#if defined(COMPONENT_FREERTOS)
    if (taskSCHEDULER_RUNNING == xTaskGetSchedulerState())
    {
        vTaskDelay(pdMS_TO_TICKS(ms));      /* yields to other tasks */
        return;
    }
#endif
    Cy_SysLib_Delay(ms);
}

uint32_t board_millis(void)
{
#if defined(COMPONENT_FREERTOS)
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
    return ms_ticks;
#endif
}

uint32_t board_led_count(void)
{
    return LED_COUNT;
}

void board_led_write(uint32_t idx, bool on)
{
    if (idx < LED_COUNT)
    {
        Cy_GPIO_Write(leds[idx].port, leds[idx].pin,
                      on ? CYBSP_LED_STATE_ON : CYBSP_LED_STATE_OFF);
    }
}

bool board_led_read(uint32_t idx)
{
    return (idx < LED_COUNT) &&
           (Cy_GPIO_ReadOut(leds[idx].port, leds[idx].pin) == CYBSP_LED_STATE_ON);
}

const char *board_led_name(uint32_t idx)
{
#if defined(BOARD_LED_NAMES)
    if (idx < LED_NAME_COUNT)
    {
        return led_names[idx];
    }
#endif
    (void)idx;
    return "LED";
}

uint32_t board_button_count(void)
{
    return BTN_COUNT;
}

bool board_button_is_pressed(uint32_t idx)
{
    return (idx < BTN_COUNT) &&
           (Cy_GPIO_Read(buttons[idx].port, buttons[idx].pin) == CYBSP_BTN_PRESSED);
}

const char *board_button_name(uint32_t idx)
{
#if defined(BOARD_BTN_NAMES)
    if (idx < BTN_NAME_COUNT)
    {
        return btn_names[idx];
    }
#endif
    (void)idx;
    return "BTN";
}

const char *board_button_pin(uint32_t idx)
{
    static char buf[12];   /* "P255.255" + NUL fits; sized for -Wformat-truncation */
    if (idx >= BTN_COUNT)
    {
        return "-";
    }
    (void)snprintf(buf, sizeof(buf), "P%u.%u", (unsigned)buttons[idx].port_num,
                   (unsigned)buttons[idx].pin);
    return buf;
}

const char *board_button_diag(uint32_t idx)
{
    static char buf[48];
    if (idx >= BTN_COUNT)
    {
        return "-";
    }
    const board_pin_t *p = &buttons[idx];
    (void)snprintf(buf, sizeof(buf), "drive=%lu hsiom=%lu in=%lu out=%lu",
                   (unsigned long)Cy_GPIO_GetDrivemode(p->port, p->pin),
                   (unsigned long)Cy_GPIO_GetHSIOM(p->port, p->pin),
                   (unsigned long)Cy_GPIO_Read(p->port, p->pin),
                   (unsigned long)Cy_GPIO_ReadOut(p->port, p->pin));
    return buf;
}

bool board_console_getc(char *c)
{
    if (0U == Cy_SCB_UART_GetNumInRxFifo(CYBSP_DEBUG_UART_HW))
    {
        return false;
    }
    *c = (char)Cy_SCB_UART_Get(CYBSP_DEBUG_UART_HW);
    return true;
}

void board_console_flush(void)
{
    while (!Cy_SCB_UART_IsTxComplete(CYBSP_DEBUG_UART_HW))
    {
    }
}

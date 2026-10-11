/*******************************************************************************
* File Name : board55.c
*
* Layer 1 - CM55 board layer (see board55.h).
* CM55 does NOT touch the debug UART: CM33 owns it for the console.
*******************************************************************************/
#include "board55.h"
#include "cybsp.h"
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

typedef struct
{
    GPIO_PRT_Type *port;
    uint32_t       pin;
} led_pin_t;

static const led_pin_t leds[BOARD55_LEDS] =
{
    { CYBSP_USER_LED1_PORT, CYBSP_USER_LED1_PIN },
    { CYBSP_USER_LED2_PORT, CYBSP_USER_LED2_PIN },
};

void board55_fatal(void)
{
    __disable_irq();
    CY_ASSERT(0);
    for (;;)
    {
    }
}

void board55_init(void)
{
    if (CY_RSLT_SUCCESS != cybsp_init())
    {
        board55_fatal();
    }
#if !defined(COMPONENT_FREERTOS)
    if (0U != SysTick_Config(SystemCoreClock / 1000U))
    {
        board55_fatal();
    }
#endif
    __enable_irq();
}

uint32_t board55_millis(void)
{
#if defined(COMPONENT_FREERTOS)
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
#else
    return ms_ticks;
#endif
}

void board55_delay_ms(uint32_t ms)
{
#if defined(COMPONENT_FREERTOS)
    if (taskSCHEDULER_RUNNING == xTaskGetSchedulerState())
    {
        vTaskDelay(pdMS_TO_TICKS(ms));
        return;
    }
#endif
    Cy_SysLib_Delay(ms);
}

bool board55_led_write(uint32_t led, bool on)
{
    if ((led < 1U) || (led > BOARD55_LEDS))
    {
        return false;
    }
    Cy_GPIO_Write(leds[led - 1U].port, leds[led - 1U].pin,
                  on ? CYBSP_LED_STATE_ON : CYBSP_LED_STATE_OFF);
    return true;
}

bool board55_led_read(uint32_t led)
{
    return (led >= 1U) && (led <= BOARD55_LEDS) &&
           (Cy_GPIO_ReadOut(leds[led - 1U].port, leds[led - 1U].pin) == CYBSP_LED_STATE_ON);
}

void board55_delay_us(uint32_t us)
{
    Cy_SysLib_DelayUs((uint16_t)us);
}

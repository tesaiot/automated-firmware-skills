/*******************************************************************************
* File Name : pinwatch.c   (template pin-watch, diagnostic)
*******************************************************************************/
#include "pinwatch.h"

#include <string.h>
#include "cy_pdl.h"

#define PORTS   (IOSS_GPIO_GPIO_PORT_NR)
#define PINS    (8U)

static uint8_t  last[PORTS];
static uint8_t  armed_mask[PORTS];
static uint8_t  muted_mask[PORTS];
static uint8_t  changes[PORTS][PINS];
static uint32_t window_start;
static uint32_t n_armed;

/* never reconfigure: level-shifted QWA309 header (P9.0-P9.3, P13, P15), analog mic
 * inputs (P14) and the crystal pins WCO (P18) / ECO (P19) - the BSP leaves the
 * crystals in analog mode, which looks like "unused" */
static bool protected_pin(uint32_t port, uint32_t pin)
{
    return (13U == port) || (14U == port) || (15U == port) || (18U == port) ||
           (19U == port) || ((9U == port) && (pin <= 3U));
}

/* only pins in plain GPIO mode can be buttons; peripheral pins (QSPI, UART RX...)
 * toggle on their own and are masked out */
static uint8_t gpio_mask(uint32_t port)
{
    GPIO_PRT_Type *base = Cy_GPIO_PortToAddr(port);
    uint8_t m = 0U;
    if (1U == port)
    {
        return 0U;      /* SMIF0 QSPI flash data: toggles with every code fetch (XIP) */
    }
    for (uint32_t i = 0U; i < PINS; i++)
    {
        if (HSIOM_SEL_GPIO == Cy_GPIO_GetHSIOM(base, i))
        {
            m |= (uint8_t)(1U << i);
        }
    }
    return m;
}

static uint8_t gpio_only[PORTS];

static uint8_t read_port(uint32_t port)
{
    return (uint8_t)(Cy_GPIO_PortToAddr(port)->IN & gpio_only[port]);
}

void pinwatch_reset(void)
{
    for (uint32_t p = 0U; p < PORTS; p++)
    {
        gpio_only[p] = gpio_mask(p);
        last[p] = read_port(p);
    }
    (void)memset(muted_mask, 0, sizeof(muted_mask));
    (void)memset(changes, 0, sizeof(changes));
    window_start = 0U;
}

uint32_t pinwatch_arm(void)
{
    for (uint32_t p = 0U; p < PORTS; p++)
    {
        GPIO_PRT_Type *base = Cy_GPIO_PortToAddr(p);
        for (uint32_t i = 0U; i < PINS; i++)
        {
            if (protected_pin(p, i) || (0U != (armed_mask[p] & (1U << i))))
            {
                continue;
            }
            if ((HSIOM_SEL_GPIO == Cy_GPIO_GetHSIOM(base, i)) &&
                (CY_GPIO_DM_ANALOG == Cy_GPIO_GetDrivemode(base, i)))
            {
                Cy_GPIO_SetDrivemode(base, i, CY_GPIO_DM_HIGHZ);   /* input on, no drive/pull */
                armed_mask[p] |= (uint8_t)(1U << i);
                n_armed++;
            }
        }
    }
    Cy_SysLib_Delay(2U);
    pinwatch_reset();
    return n_armed;
}

void pinwatch_disarm(void)
{
    for (uint32_t p = 0U; p < PORTS; p++)
    {
        for (uint32_t i = 0U; i < PINS; i++)
        {
            if (0U != (armed_mask[p] & (1U << i)))
            {
                Cy_GPIO_SetDrivemode(Cy_GPIO_PortToAddr(p), i, CY_GPIO_DM_ANALOG);
            }
        }
        armed_mask[p] = 0U;
    }
    n_armed = 0U;
    pinwatch_reset();
}

uint32_t pinwatch_armed(void)
{
    return n_armed;
}

uint32_t pinwatch_muted(void)
{
    uint32_t n = 0U;
    for (uint32_t p = 0U; p < PORTS; p++)
    {
        for (uint32_t i = 0U; i < PINS; i++)
        {
            n += (muted_mask[p] >> i) & 1U;
        }
    }
    return n;
}

void pinwatch_poll(uint32_t now_ms, pinwatch_cb_t cb)
{
    if ((0U == window_start) || ((now_ms - window_start) >= 1000U))
    {
        (void)memset(changes, 0, sizeof(changes));
        window_start = (0U == now_ms) ? 1U : now_ms;
    }
    for (uint32_t p = 0U; p < PORTS; p++)
    {
        uint8_t now = read_port(p);
        uint8_t diff = (uint8_t)((now ^ last[p]) & (uint8_t)~muted_mask[p]);
        last[p] = now;
        for (uint32_t i = 0U; (0U != diff) && (i < PINS); i++)
        {
            if (0U == (diff & (1U << i)))
            {
                continue;
            }
            bool mute = (++changes[p][i] > PINWATCH_NOISY_PER_S);
            if (mute)
            {
                muted_mask[p] |= (uint8_t)(1U << i);
            }
            if (NULL != cb)
            {
                cb(p, i, 0U != (now & (1U << i)), mute);
            }
        }
    }
}

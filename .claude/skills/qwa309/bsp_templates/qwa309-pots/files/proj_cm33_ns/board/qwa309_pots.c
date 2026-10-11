/*******************************************************************************
* File Name : qwa309_pots.c                   component: qwa309-pots
*
* Layer 1 - BSP (QWA309). The vendor KIT_PSE84_AI BSP 1.4.0 already generates an
* Autonomous Analog configuration with one SAR GPIO channel (P15.1); its config
* structures are not const, so instead of a design.modus overlay this driver
* points the high-speed GPIO channels at P15.4..P15.7 (SAR channels 4..7) before
* calling Cy_AutAnalog_Init(), then starts the autonomous controller. The SAR
* then converts continuously; reads are plain result-register loads.
* Guideline (never built here): TESAIoT SDK mod_qwa309_pots.c + CM33 main.c
* init_potentiometer_adc() - same channels, start sequence and readiness check.
*******************************************************************************/
#include "qwa309_pots.h"

#include "cybsp.h"
#include "qwa309.h"            /* compile-time board-scope gate + pin facts */

#define POTS_SAR_IDX        (0U)
#define POTS_FIRST_CH       (4U)
#define POTS_CH_MASK        (CY_AUTANALOG_SAR_CHAN_MASK_GPIO4 | CY_AUTANALOG_SAR_CHAN_MASK_GPIO5 | \
                             CY_AUTANALOG_SAR_CHAN_MASK_GPIO6 | CY_AUTANALOG_SAR_CHAN_MASK_GPIO7)

static const uint8_t knob_ch[POTS_COUNT] = QWA309_POT_CHANNELS;   /* VR1..VR4 -> SAR ch */
static const char *const knob_names[POTS_COUNT] = { "VR1", "VR2", "VR3", "VR4" };
static const char *const ch_pins[8] = { "P15.0", "P15.1", "P15.2", "P15.3",
                                        "P15.4", "P15.5", "P15.6", "P15.7" };

static cy_stc_autanalog_sar_hs_chan_t pot_chan[POTS_COUNT];       /* SAR ch 4..7 */
static bool     started;
static uint32_t last_rslt;

bsp_status_t pots_init(void)
{
    static const cy_en_autanalog_sar_pin_hs_t pins[POTS_COUNT] =
    {
        CY_AUTANALOG_SAR_PIN_GPIO4, CY_AUTANALOG_SAR_PIN_GPIO5,
        CY_AUTANALOG_SAR_PIN_GPIO6, CY_AUTANALOG_SAR_PIN_GPIO7
    };
    if (started)
    {
        return BSP_OK;
    }
    /* P15.4..P15.7 as analog inputs (high-Z, GPIO HSIOM) - never 3.3 V on these pins */
    GPIO_PRT_Type *port = Cy_GPIO_PortToAddr(QWA309_POT_PORT_NUM);
    for (uint32_t i = 0U; i < POTS_COUNT; i++)
    {
        Cy_GPIO_SetHSIOM(port, POTS_FIRST_CH + i, HSIOM_SEL_GPIO);
        Cy_GPIO_SetDrivemode(port, POTS_FIRST_CH + i, CY_GPIO_DM_ANALOG);
    }
    /* SAR high-speed GPIO channels: only 4..7, single-ended against GPIO0 (unused) */
    for (uint32_t i = 0U; i < POTS_COUNT; i++)
    {
        pot_chan[i] = (cy_stc_autanalog_sar_hs_chan_t)
        {
            .posPin   = pins[i],
            .hsDiffEn = false,
            .sign     = false,
            .posCoeff = CY_AUTANALOG_SAR_CH_COEFF_DISABLED,
            .negPin   = CY_AUTANALOG_SAR_PIN_GPIO0,
            .accShift = false,
            .negCoeff = CY_AUTANALOG_SAR_CH_COEFF_DISABLED,
            .hsLimit  = CY_AUTANALOG_SAR_LIMIT_STATUS_DISABLED,
            .fifoSel  = CY_AUTANALOG_FIFO_DISABLED,
        };
    }
    for (uint32_t ch = 0U; ch < 8U; ch++)
    {
        CYBSP_SAR_ADC_sta_hs_cfg.hsGpioChan[ch] = NULL;
    }
    for (uint32_t i = 0U; i < POTS_COUNT; i++)
    {
        CYBSP_SAR_ADC_sta_hs_cfg.hsGpioChan[POTS_FIRST_CH + i] = &pot_chan[i];
    }
    CYBSP_SAR_ADC_sta_hs_cfg.hsGpioResultMask = POTS_CH_MASK;
    CYBSP_SAR_ADC_seq_hs_cfg[0].chanEn = POTS_CH_MASK;

    uint32_t r = Cy_AutAnalog_Init(&autonomous_analog_init);
    if (CY_AUTANALOG_SUCCESS != r)
    {
        last_rslt = r;
        return BSP_ERR_INIT;
    }
    Cy_AutAnalog_StartAutonomousControl();
    started = true;
    return BSP_OK;
}

bool pots_ready(void)
{
    return started &&
           (POTS_CH_MASK == (Cy_AutAnalog_SAR_GetHSchanResultStatus(POTS_SAR_IDX) & POTS_CH_MASK));
}

bsp_status_t pots_read_raw(uint32_t knob, uint16_t *count)
{
    if ((knob >= POTS_COUNT) || (NULL == count))
    {
        return BSP_ERR_PARAM;
    }
    if (!started)
    {
        return BSP_ERR_INIT;
    }
    int32_t v = Cy_AutAnalog_SAR_ReadResult(POTS_SAR_IDX, CY_AUTANALOG_SAR_INPUT_GPIO, knob_ch[knob]);
    if (v < 0)
    {
        v = 0;
    }
    else if (v > (int32_t)POTS_MAX_COUNT)
    {
        v = (int32_t)POTS_MAX_COUNT;
    }
    *count = (uint16_t)v;
    return BSP_OK;
}

bsp_status_t pots_read_mv(uint32_t knob, uint32_t *mv)
{
    uint16_t c = 0U;
    bsp_status_t s = pots_read_raw(knob, &c);
    if ((BSP_OK == s) && (NULL != mv))
    {
        *mv = ((uint32_t)c * POTS_FULL_SCALE_MV + (POTS_MAX_COUNT / 2U)) / POTS_MAX_COUNT;
    }
    return (NULL == mv) ? BSP_ERR_PARAM : s;
}

bsp_status_t pots_read_permille(uint32_t knob, uint32_t *permille)
{
    uint16_t c = 0U;
    bsp_status_t s = pots_read_raw(knob, &c);
    if ((BSP_OK == s) && (NULL != permille))
    {
        *permille = ((uint32_t)c * 1000U + (POTS_MAX_COUNT / 2U)) / POTS_MAX_COUNT;
    }
    return (NULL == permille) ? BSP_ERR_PARAM : s;
}

const char *pots_name(uint32_t knob)
{
    return (knob < POTS_COUNT) ? knob_names[knob] : "?";
}

const char *pots_pin(uint32_t knob)
{
    return (knob < POTS_COUNT) ? ch_pins[knob_ch[knob]] : "?";
}

uint32_t pots_last_rslt(void)
{
    return last_rslt;
}

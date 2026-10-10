/*******************************************************************************
* File Name : mic_analog.h                    component: mic-analog (planned)
*
* Layer 1 - BSP. Two Infineon XENSIV IM73A135 analog MEMS microphones (U10 left,
*   U9 right, ~32 mm apart, 1.8 V) of the PSOC Edge E84 AI Kit, wired to the
*   Autonomous Analog CTB opamps (port 14, 1.8 V):
*     AMIC1: INP P14.0, INN P14.1, OUT P14.2, REF P14.6
*     AMIC2: INP P14.4, INN P14.5, OUT P14.3, REF P14.7
*   Opamp output -> SAR ADC (Autonomous Analog). Needs a design.modus overlay
*   (CTB + SAR scan); no Infineon code example found for these mics yet.
* Claims SAR: on TESAIoT the QWA309 pots also need SAR -> the gate blocks
*   mic-analog + qwa309-pots until a shared SAR-scan component arbitrates SAR.
*******************************************************************************/
#ifndef MIC_ANALOG_H
#define MIC_ANALOG_H

#include <stdint.h>
#include "bsp_status.h"

typedef struct
{
    uint32_t sample_rate_hz;   /* 8000 or 16000 (default) */
    uint8_t  gain_db;          /* CTB gain, 0..20 dB      */
} amic_config_t;

#define AMIC_CONFIG_DEFAULT  { 16000UL, 12U }

bsp_status_t amic_init(const amic_config_t *cfg);
void         amic_deinit(void);
bsp_status_t amic_start(void);
bsp_status_t amic_stop(void);

/* Interleaved AMIC1, AMIC2 16-bit signed samples (DC removed). */
uint32_t     amic_read(int16_t *pcm, uint32_t max_frames, bsp_status_t *status);
bsp_status_t amic_level(uint32_t frames, uint16_t *rms_1, uint16_t *rms_2);

#endif /* MIC_ANALOG_H */

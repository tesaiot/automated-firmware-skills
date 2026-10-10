/*******************************************************************************
* File Name : audio_out.h                     component: audio-tlv320dac3100 (planned)
*
* Layer 1 - BSP. Audio output of the PSOC Edge E84 AI Kit: stereo audio DAC with
*   Class-D BTL mono speaker driver (up to 2.5 W, connector J8 - NOT fitted by
*   default) and Class-A/B headphone driver (U28). The UG gives no part number;
*   I2C 0x18 + the kit's Infineon example (mtb-example-psoc-edge-i2s) use the
*   TI TLV320DAC3100 library -> treated as TLV320DAC3100 (verify on hardware).
*   Control: I2C SCB0 @0x18 (i2c_scb0.h). Data: I2S TX on TDM0
*   (CYBSP_TDM_CONTROLLER_0): MCK P21.3, SCK P21.2, SD P21.1, FSYNC P12.3, 1.8 V.
*   Supplies: DVDD/IOVDD 1.8 V, AVDD/HPVDD 3.3 V, SPKVDD 5 V.
* Backend: Infineon audio-codec-tlv320dac3100 (mtb_tlv320dac3100_init /
*   _configure_clocking / _activate / _adjust_speaker_output_volume) + PDL
*   Cy_AudioTDM_* TX FIFO IRQ fed from a ring buffer.
*******************************************************************************/
#ifndef AUDIO_OUT_H
#define AUDIO_OUT_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

typedef enum
{
    AUDIO_OUT_SPEAKER   = 0,   /* J8, mono mix of L+R */
    AUDIO_OUT_HEADPHONE,
    AUDIO_OUT_BOTH
} audio_out_route_t;

typedef struct
{
    uint32_t          sample_rate_hz;  /* 8000, 16000, 44100, 48000 (default) */
    uint8_t           word_bits;       /* 16 (default), 24, 32                */
    audio_out_route_t route;           /* default AUDIO_OUT_SPEAKER           */
    uint8_t           volume_pct;      /* 0..100, default 50                  */
} audio_out_config_t;

#define AUDIO_OUT_CONFIG_DEFAULT  { 48000UL, 16U, AUDIO_OUT_SPEAKER, 50U }

/* Needs i2c0_init() first. Configures DAC clocking from MCLK, starts TDM TX
 * (outputs zeros until data is written). */
bsp_status_t audio_out_init(const audio_out_config_t *cfg);
void         audio_out_deinit(void);
bsp_status_t audio_out_set_volume(uint8_t volume_pct);
bsp_status_t audio_out_mute(bool mute);

/* Queue interleaved stereo PCM (L,R). Returns frames accepted (ring buffer
 * space); never blocks. */
uint32_t     audio_out_write(const int16_t *pcm, uint32_t frames);
uint32_t     audio_out_free_frames(void);
uint32_t     audio_out_underruns(void);      /* FIFO underflow count          */

/* Test helper: sine tone, freq_hz for ms (blocking-free, generated in IRQ). */
bsp_status_t audio_out_tone(uint32_t freq_hz, uint32_t ms);

uint32_t     audio_out_last_rslt(void);

#endif /* AUDIO_OUT_H */

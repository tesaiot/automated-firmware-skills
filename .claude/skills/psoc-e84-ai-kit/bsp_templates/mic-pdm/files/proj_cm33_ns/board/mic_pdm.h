/*******************************************************************************
* File Name : mic_pdm.h                       component: mic-pdm (planned)
*
* Layer 1 - BSP. Two Infineon XENSIV IM73D122 digital MEMS microphones (U7 right,
*   U8 left, ~27 mm apart) of the PSOC Edge E84 AI Kit, one PDM bus:
*   PDM_CLK P8.5, PDM_DATA P8.6 (1.8 V). Left = data on falling edge
*   (BSP PDM0 channel 2), right = rising edge (channel 3).
* Backend: PDL PDM-PCM (Cy_PDM_PCM_Init(PDM0, &CYBSP_PDM_config),
*   Cy_PDM_PCM_Channel_Init(channel_2_config / channel_3_config), FIFO IRQ
*   CYBSP_PDM_CHANNEL_3_IRQ) as in mtb-example-psoc-edge-pdm-pcm.
* Data: 16-bit PCM, interleaved L,R. The driver fills a ring buffer from the
*   FIFO IRQ; mic_read() copies out (non-blocking).
*******************************************************************************/
#ifndef MIC_PDM_H
#define MIC_PDM_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

typedef enum
{
    MIC_CH_LEFT   = 1U,   /* U8  */
    MIC_CH_RIGHT  = 2U,   /* U7  */
    MIC_CH_STEREO = 3U
} mic_channels_t;

typedef struct
{
    uint32_t       sample_rate_hz;  /* 8000, 16000 (default), 48000          */
    mic_channels_t channels;        /* default MIC_CH_STEREO                 */
    int8_t         gain_db;         /* -12 .. +10.5 dB in PDL steps, default 0 */
} mic_config_t;

#define MIC_CONFIG_DEFAULT  { 16000UL, MIC_CH_STEREO, 0 }

bsp_status_t mic_init(const mic_config_t *cfg);   /* NULL = default            */
void         mic_deinit(void);
bsp_status_t mic_start(void);                     /* activate channels         */
bsp_status_t mic_stop(void);
bsp_status_t mic_set_gain(int8_t gain_db);

/* Copy up to max_frames frames (1 frame = 1 sample per enabled channel,
 * interleaved). Returns frames copied; *status = BSP_ERR_OVERFLOW once after
 * the ring buffer overran. */
uint32_t     mic_read(int16_t *pcm, uint32_t max_frames, bsp_status_t *status);
uint32_t     mic_available(void);                 /* frames buffered           */

/* Test helper: RMS level of the last N frames per channel, in 0..32767. */
bsp_status_t mic_level(uint32_t frames, uint16_t *rms_left, uint16_t *rms_right);

#endif /* MIC_PDM_H */

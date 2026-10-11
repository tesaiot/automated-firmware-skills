/*******************************************************************************
* File Name : display.c                       component: display-dsi
*
* Layer 1 - BSP (CM55). See display.h. Guidelines (read, rewritten, never
* copied): Infineon mtb-example-psoc-edge-gfx-lvgl-demo (init order, GPU heap),
* E84_TouchUI (github.com/NnopponS/WheelSense_Project, firmware/E84_TouchUI),
* TESAIoT SDK proj_cm55/gfxss_config.c (config values) + its comments (bus clear
* before panel init, missed-interrupt watchdog). Bare metal: the display
* controller interrupt sets a flag instead of an RTOS notify.
*******************************************************************************/
#include "display.h"

#include <string.h>
#include "cybsp.h"
#include "cy_graphics.h"
#include "vg_lite.h"
#include "vg_lite_platform.h"
#include "mtb_disp_dsi_waveshare_4p3.h"
#include "board55.h"
#include "i2c_scb5.h"

#define DC_IRQ_PRIORITY     (3U)
#define I2C_IRQ_PRIORITY    (2U)
#define GFXSS_CLOCK_HZ      (399999999UL)   /* HFCLK1 = 400 MHz; value as in the SDK config */
#define PANEL_TRIES         (3U)
#define PANEL_SETTLE_MS     (100U)

/* gfx_mem (0x1C4000 B) = 2 frame buffers + the VG-Lite command heap */
#define GPU_HEAP_BYTES      (0x1C4000UL - (DISPLAY_FB_COUNT * DISPLAY_FB_BYTES))

static uint8_t fb[DISPLAY_FB_COUNT][DISPLAY_FB_BYTES] CY_SECTION(".cy_gpu_buf") __attribute__((aligned(128)));
static uint8_t gpu_heap[GPU_HEAP_BYTES] CY_SECTION(".cy_gpu_buf") __attribute__((aligned(64)));

static cy_stc_gfx_context_t gfx_ctx;

static cy_stc_gfx_layer_config_t layer_gfx =
{
    .layer_type        = GFX_LAYER_GRAPHICS,
    .input_format_type = vivRGB565,
    .tiling_type       = vivLINEAR,
    .width             = DISPLAY_STRIDE_PX,
    .height            = DISPLAY_HEIGHT,
    .layer_enable      = true,
    .visibility        = true,
};
static cy_stc_gfx_layer_config_t layer_ovl0 =
{
    .layer_type = GFX_LAYER_OVERLAY0, .input_format_type = vivRGB565, .tiling_type = vivLINEAR,
    .width = 1U, .height = 1U,
};
static cy_stc_gfx_layer_config_t layer_ovl1 =
{
    .layer_type = GFX_LAYER_OVERLAY1, .input_format_type = vivRGB565, .tiling_type = vivLINEAR,
    .width = 1U, .height = 1U,
};
static cy_stc_gfx_dc_config_t dc_cfg =
{
    .gfx_layer_config  = &layer_gfx,
    .ovl0_layer_config = &layer_ovl0,
    .ovl1_layer_config = &layer_ovl1,
    .display_type      = GFX_DISP_TYPE_DSI_DPI,
    .display_format    = vivD24,
    .display_size      = vivDISPLAY_CUSTOMIZED,
    .display_width     = DISPLAY_STRIDE_PX,
    .display_height    = DISPLAY_HEIGHT,
};
static cy_stc_gfx_gpu_cfg_t gpu_cfg = { .enable = true };
static cy_stc_gfx_config_t gfx_cfg =
{
    .dc_cfg              = &dc_cfg,
    .gpu_cfg             = &gpu_cfg,
    .mipi_dsi_cfg        = &mtb_disp_waveshare_4p3_dsi_config,   /* panel timing from the library */
    .display_update_type = GFX_DOUBLE_BUFFER,
    .clockHz             = GFXSS_CLOCK_HZ,
};

static volatile bool     shown = true;
static volatile uint32_t irq_count;
static uint32_t          show_count;
static uint32_t          timeout_count;
static uint32_t          last_rslt;
static bool              ready;

static void dc_isr(void)
{
    Cy_GFXSS_Clear_DC_Interrupt(GFXSS, &gfx_ctx);
    irq_count++;
    shown = true;
}

static bsp_status_t panel_on(void)
{
    cy_en_scb_i2c_status_t r = CY_SCB_I2C_SUCCESS;
    for (uint32_t i = 0U; i < PANEL_TRIES; i++)
    {
        board55_delay_ms(PANEL_SETTLE_MS);       /* panel MCU drops writes while it powers up */
        r = mtb_disp_waveshare_4p3_init((CySCB_Type *)i2c5_scb_base(),
                                        (cy_stc_scb_i2c_context_t *)i2c5_scb_context());
        if (CY_SCB_I2C_SUCCESS == r)
        {
            return BSP_OK;
        }
    }
    last_rslt = (uint32_t)r;
    return BSP_ERR_NODEV;
}

bsp_status_t display_init(void)
{
    if (!i2c5_is_ready() && (BSP_OK != i2c5_init(I2C5_DEFAULT_HZ)))
    {
        last_rslt = i2c5_last_rslt();
        return BSP_ERR_INIT;
    }
    /* a warm reset can leave a panel/touch transfer half-done on SCB5 (SDK: black
     * screen 3/6 -> 0/27 after a bus clear) */
    (void)i2c5_recover();
    if (BSP_OK != i2c5_irq_enable(I2C_IRQ_PRIORITY))
    {
        return BSP_ERR_INIT;
    }

    (void)memset(fb, 0, sizeof(fb));            /* black */
    layer_gfx.buffer_address    = (gctADDRESS *)fb[0];
    layer_gfx.uv_buffer_address = (gctADDRESS *)fb[0];
    cy_en_gfx_status_t g = Cy_GFXSS_Init(GFXSS, &gfx_cfg, &gfx_ctx);
    if (CY_GFX_SUCCESS != g)
    {
        last_rslt = (uint32_t)g;
        return BSP_ERR_INIT;
    }

    const cy_stc_sysint_t dc_irq = { .intrSrc = gfxss_interrupt_dc_IRQn, .intrPriority = DC_IRQ_PRIORITY };
    if (CY_SYSINT_SUCCESS != Cy_SysInt_Init(&dc_irq, dc_isr))
    {
        return BSP_ERR_INIT;
    }
    NVIC_ClearPendingIRQ(gfxss_interrupt_dc_IRQn);
    NVIC_EnableIRQ(gfxss_interrupt_dc_IRQn);

    bsp_status_t s = panel_on();
    ready = (BSP_OK == s);
    return s;
}

bsp_status_t display_gpu_init(void)
{
    if (!ready)
    {
        return BSP_ERR_INIT;
    }
    /* Bare metal: the VG-Lite kernel polls the GPU interrupt status itself
     * (_BAREMETAL default 1), so no NVIC handler is installed for the GPU. */
    vg_module_parameters_t p;
    (void)memset(&p, 0, sizeof(p));
    p.register_mem_base      = (uint32_t)GFXSS_GFXSS_GPU_GCNANO;
    p.gpu_mem_base[0]        = 0U;
    p.contiguous_mem_base[0] = gpu_heap;
    p.contiguous_mem_size[0] = sizeof(gpu_heap);
    vg_lite_init_mem(&p);
    vg_lite_error_t e = vg_lite_init((int32_t)DISPLAY_STRIDE_PX, (int32_t)DISPLAY_HEIGHT);
    if (VG_LITE_SUCCESS != e)
    {
        last_rslt = (uint32_t)e;
        return BSP_ERR_INIT;
    }
    return BSP_OK;
}

void *display_framebuffer(uint32_t index)
{
    return (index < DISPLAY_FB_COUNT) ? (void *)fb[index] : NULL;
}

void display_show(const void *buffer)
{
    shown = false;
    __DSB();                                    /* pixels written before the switch */
    cy_en_gfx_status_t g = Cy_GFXSS_Set_FrameBuffer(GFXSS, (uint32_t *)buffer, &gfx_ctx);
    if (CY_GFX_SUCCESS != g)
    {
        last_rslt = (uint32_t)g;
        shown = true;                           /* nothing pending */
    }
    show_count++;
}

bool display_shown(void)
{
    return shown;
}

bool display_wait_shown(uint32_t timeout_ms)
{
    uint32_t t0 = board55_millis();
    while (!shown)
    {
        if ((board55_millis() - t0) >= timeout_ms)
        {
            timeout_count++;
            shown = true;                       /* SDK guideline: never freeze on a missed IRQ */
            return false;
        }
        __WFI();                                /* SysTick or the DC interrupt wakes us */
    }
    return true;
}

bsp_status_t display_set_brightness(uint8_t percent)
{
    cy_en_scb_i2c_status_t r = mtb_disp_waveshare_4p3_set_brightness(
        (CySCB_Type *)i2c5_scb_base(), (cy_stc_scb_i2c_context_t *)i2c5_scb_context(),
        (percent > 100U) ? 100U : percent);
    if (CY_SCB_I2C_SUCCESS != r)
    {
        last_rslt = (uint32_t)r;
        return BSP_ERR_BUS;
    }
    return BSP_OK;
}

uint32_t display_irq_count(void)     { return irq_count; }
uint32_t display_show_count(void)    { return show_count; }
uint32_t display_timeout_count(void) { return timeout_count; }
uint32_t display_last_rslt(void)     { return last_rslt; }

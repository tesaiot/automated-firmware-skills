/*******************************************************************************
* File Name : main.c  (CM33 non-secure)
*
* Layer 3 - application. qwa309-io (board tesaiot, scope cm33, bare metal):
* QWA309 base-board inputs - potentiometers VR1..VR4 (SAR on P15.4..P15.7) and
* the CapSense buttons/slider (PSoC 4000T @0x08 on SCB5).
* Output (key=value; tests parse it):
*   every report period : POTS vr1=1234 vr2=... (raw 0..4095) ready=1
*                         CAPS btn0=0 btn1=0 slider=0 raw=31,31,0
*   on change           : EVT pot VR1 raw=2048 mv=900 pct=50
*                         EVT caps btn0=pressed|released   EVT caps slider=42
* Commands: info, pots, caps, scan5, report on|off, bus5, reinit, i2c5hz, campins,
* ezi2c [n], ezwatch on|off, ezreport <ms> (raw 4000T buffer).
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cybsp.h"            /* CYBSP_DVP_CAM_RESET/PWDN (campins) */
#include "board.h"
#include "console.h"
#include "i2c_scb5.h"
#include "qwa309_pots.h"
#include "qwa309_capsense.h"
#include "qwa309.h"            /* QWA309_I2C_ADDR_CAPSENSE */

#define APP_NAME            "qwa309-io"
#define APP_VERSION         "1.0"
#define REPORT_PERIOD_MS    (1000U)
#define POLL_PERIOD_MS      (50U)
#define POT_HYST_COUNTS     (80U)     /* ~2 % of full scale before an EVT */
#define SLIDER_HYST         (5U)

static bsp_status_t st_bus, st_pots, st_caps;
static bool         report_on = true;
static uint16_t     pot_last[POTS_COUNT];
static capsense_state_t caps_last;

/* raw view of the PSoC 4000T EZI2C buffer (@0x08): does it carry more than the
 * 3 documented bytes, e.g. the base-board push buttons or DIP switches? */
#define EZ_MAX              (64U)
#define EZ_WATCH_BYTES      (32U)
static bool         ez_watch;
static bool         ez_valid;
static uint8_t      ez_last[EZ_WATCH_BYTES];

static void print_pots(void)
{
    uint16_t v[POTS_COUNT] = { 0U };
    if (BSP_OK != st_pots)
    {
        printf("POTS ERR %s\r\n", bsp_status_str(st_pots));
        return;
    }
    for (uint32_t i = 0U; i < POTS_COUNT; i++)
    {
        (void)pots_read_raw(i, &v[i]);
    }
    printf("POTS vr1=%u vr2=%u vr3=%u vr4=%u ready=%u\r\n", v[0], v[1], v[2], v[3],
           pots_ready() ? 1U : 0U);
}

static void print_caps(void)
{
    capsense_state_t s;
    bsp_status_t r = (BSP_OK == st_caps) ? capsense_read(&s) : st_caps;
    if ((BSP_OK == r) && (CAPSENSE_FMT_RAW5 == capsense_format()))
    {
        /* 5-segment slider firmware: no buttons; d0..d4 = drop per segment */
        printf("CAPS touch=%u slider=%u d0=%d d1=%d d2=%d d3=%d d4=%d fmt=raw5\r\n",
               s.slider_touched ? 1U : 0U, s.slider, s.drop[0], s.drop[1], s.drop[2],
               s.drop[3], s.drop[4]);
    }
    else if (BSP_OK == r)
    {
        printf("CAPS btn0=%u btn1=%u slider=%u raw=%u,%u,%u fmt=frame3\r\n", s.btn0 ? 1U : 0U,
               s.btn1 ? 1U : 0U, s.slider, s.raw[0], s.raw[1], s.raw[2]);
    }
    else
    {
        printf("CAPS ERR %s rslt=0x%08lX\r\n", bsp_status_str(r), (unsigned long)capsense_last_rslt());
    }
}

/* events: knob moved past the hysteresis, pad pressed/released, slider moved */
static void poll_inputs(void)
{
    if (BSP_OK == st_pots)
    {
        for (uint32_t i = 0U; i < POTS_COUNT; i++)
        {
            uint16_t v = 0U;
            uint32_t mv = 0U, pm = 0U;
            if ((BSP_OK == pots_read_raw(i, &v)) &&
                ((uint32_t)abs((int)v - (int)pot_last[i]) >= POT_HYST_COUNTS))
            {
                pot_last[i] = v;
                (void)pots_read_mv(i, &mv);
                (void)pots_read_permille(i, &pm);
                printf("EVT pot %s raw=%u mv=%lu pct=%lu pin=%s\r\n", pots_name(i), v,
                       (unsigned long)mv, (unsigned long)(pm / 10U), pots_pin(i));
            }
        }
    }
    capsense_state_t s;
    if ((BSP_OK == st_caps) && (BSP_OK == capsense_read(&s)))
    {
        if (s.btn0 != caps_last.btn0)
        {
            printf("EVT caps btn0=%s\r\n", s.btn0 ? "pressed" : "released");
        }
        if (s.btn1 != caps_last.btn1)
        {
            printf("EVT caps btn1=%s\r\n", s.btn1 ? "pressed" : "released");
        }
        if (s.slider_touched != caps_last.slider_touched)
        {
            printf("EVT caps slider=%s pos=%u\r\n", s.slider_touched ? "touched" : "released",
                   s.slider);
            caps_last.slider = s.slider;
        }
        else if (s.slider_touched &&
                 ((uint32_t)abs((int)s.slider - (int)caps_last.slider) >= SLIDER_HYST))
        {
            printf("EVT caps slider=%u\r\n", s.slider);
            caps_last.slider = s.slider;
        }
        caps_last.btn0 = s.btn0;
        caps_last.btn1 = s.btn1;
        caps_last.slider_touched = s.slider_touched;
    }
}

static void print_info(void)
{
    printf("INFO app=%s v=%s board=%s scope=cm33 i2c5=%s pots=%s caps=%s capsfmt=%s\r\n", APP_NAME,
           APP_VERSION, BOARD_NAME, bsp_status_str(st_bus), bsp_status_str(st_pots),
           bsp_status_str(st_caps), capsense_format_name());
    printf("READY\r\n");
}

static void cmd_info(int argc, char *argv[])   { (void)argc; (void)argv; print_info(); }
static void cmd_pots(int argc, char *argv[])   { (void)argc; (void)argv; print_pots(); }
static void cmd_caps(int argc, char *argv[])   { (void)argc; (void)argv; print_caps(); }

static void cmd_scan5(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint8_t found[16];
    if (BSP_OK != st_bus)
    {
        printf("ERR i2c5 %s\r\n", bsp_status_str(st_bus));
        return;
    }
    uint32_t n = i2c5_scan(found, (uint32_t)sizeof(found));
    printf("SCAN bus=i2c5 n=%lu", (unsigned long)n);
    for (uint32_t i = 0U; (i < n) && (i < sizeof(found)); i++)
    {
        printf(" 0x%02X", (unsigned)found[i]);
    }
    printf("\r\n");
}

static void cmd_report(int argc, char *argv[])
{
    if ((argc > 1) && ((0 == strcmp(argv[1], "on")) || (0 == strcmp(argv[1], "off"))))
    {
        report_on = (0 == strcmp(argv[1], "on"));
    }
    printf("OK report=%s\r\n", report_on ? "on" : "off");
}

/* bus diagnostics: line levels on the idle bus, then a recovery attempt */
static void cmd_bus5(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    bool scl = false, sda = false;
    i2c5_lines(&scl, &sda);
    bsp_status_t r = i2c5_recover();
    bool scl2 = false, sda2 = false;
    i2c5_lines(&scl2, &sda2);
    printf("BUS5 scl=%u sda=%u recover=%s after: scl=%u sda=%u\r\n", scl ? 1U : 0U, sda ? 1U : 0U,
           bsp_status_str(r), scl2 ? 1U : 0U, sda2 ? 1U : 0U);
    bool pscl = false, psda = false;
    st_bus = i2c5_pullup_probe(&pscl, &psda);
    printf("BUS5 internal-pullup: scl=%u sda=%u (%s)\r\n", pscl ? 1U : 0U, psda ? 1U : 0U,
           (pscl && psda) ? "lines free (not held low)"
                          : "a line is held low by a device");
}

/* camera control nets P17.5 RESET / P17.7 PWDN (= SW4/SW5): "bsp" drives them as the
 * vendor BSP and the TESAIoT SDK firmware do (RESET=1, PWDN=0 -> camera awake, out of
 * reset); "buttons" = pulled-up inputs (board_init default on tesaiot) */
static void cmd_campins(int argc, char *argv[])
{
    if ((argc > 1) && (0 == strcmp(argv[1], "bsp")))
    {
        (void)Cy_GPIO_Pin_Init(CYBSP_DVP_CAM_RESET_PORT, CYBSP_DVP_CAM_RESET_PIN, &CYBSP_DVP_CAM_RESET_config);
        (void)Cy_GPIO_Pin_Init(CYBSP_DVP_CAM_PWDN_PORT, CYBSP_DVP_CAM_PWDN_PIN, &CYBSP_DVP_CAM_PWDN_config);
    }
    else if ((argc > 1) && (0 == strcmp(argv[1], "buttons")))
    {
        Cy_GPIO_Pin_FastInit(CYBSP_DVP_CAM_RESET_PORT, CYBSP_DVP_CAM_RESET_PIN, CY_GPIO_DM_PULLUP, 1U, HSIOM_SEL_GPIO);
        Cy_GPIO_Pin_FastInit(CYBSP_DVP_CAM_PWDN_PORT, CYBSP_DVP_CAM_PWDN_PIN, CY_GPIO_DM_PULLUP, 1U, HSIOM_SEL_GPIO);
    }
    else
    {
        printf("ERR usage: campins bsp|buttons\r\n");
        return;
    }
    printf("OK campins=%s reset_out=%lu pwdn_out=%lu\r\n", argv[1],
           (unsigned long)Cy_GPIO_ReadOut(CYBSP_DVP_CAM_RESET_PORT, CYBSP_DVP_CAM_RESET_PIN),
           (unsigned long)Cy_GPIO_ReadOut(CYBSP_DVP_CAM_PWDN_PORT, CYBSP_DVP_CAM_PWDN_PIN));
}

static void cmd_ezi2c(int argc, char *argv[])
{
    uint8_t  b[EZ_MAX];
    uint32_t n = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 16U;
    if ((n < 1U) || (n > EZ_MAX))
    {
        printf("ERR usage: ezi2c [1..%u]\r\n", (unsigned)EZ_MAX);
        return;
    }
    bsp_status_t r = i2c5_read(QWA309_I2C_ADDR_CAPSENSE, b, n);
    if (BSP_OK != r)
    {
        printf("EZ ERR %s rslt=0x%08lX\r\n", bsp_status_str(r), (unsigned long)i2c5_last_rslt());
        return;
    }
    printf("EZ n=%lu", (unsigned long)n);
    for (uint32_t i = 0U; i < n; i++)
    {
        printf(" %02X", b[i]);
    }
    printf("\r\n");
}

/* periodic raw view for the live page: the SDK 3-byte frame with the SDK button decode
 * (pressed = code differs from the code seen when ezreport started) and words 0..22 of
 * the buffer as unsigned 16-bit numbers (words 23..31 never change) */
#define EZ_REPORT_WORDS     (23U)
static uint32_t ez_report_ms;
static uint32_t ez_next;
static uint8_t  ez_idle0, ez_idle1;
static bool     ez_idle_valid;

static void report_ez(uint32_t now)
{
    uint8_t b[EZ_REPORT_WORDS * 2U];
    if ((0U == ez_report_ms) || ((int32_t)(now - ez_next) < 0) || (BSP_OK != st_bus))
    {
        return;
    }
    ez_next = now + ez_report_ms;
    if (BSP_OK != i2c5_read(QWA309_I2C_ADDR_CAPSENSE, b, sizeof(b)))
    {
        printf("FRAME ERR rslt=0x%08lX\r\n", (unsigned long)i2c5_last_rslt());
        return;
    }
    if (!ez_idle_valid)
    {
        ez_idle0 = capsense_sdk_code(b[0]);
        ez_idle1 = capsense_sdk_code(b[1]);
        ez_idle_valid = true;
    }
    printf("FRAME b0=%u b1=%u b2=%u sdk_btn0=%u sdk_btn1=%u\r\n", b[0], b[1], b[2],
           (capsense_sdk_code(b[0]) != ez_idle0) ? 1U : 0U,
           (capsense_sdk_code(b[1]) != ez_idle1) ? 1U : 0U);
    printf("EZW");
    for (uint32_t i = 0U; i < EZ_REPORT_WORDS; i++)
    {
        printf(" w%lu=%u", (unsigned long)i, (unsigned)((uint16_t)b[2U * i] | ((uint16_t)b[(2U * i) + 1U] << 8)));
    }
    printf("\r\n");
}

static void cmd_ezreport(int argc, char *argv[])
{
    ez_report_ms  = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : 0U;
    ez_report_ms  = ((ez_report_ms > 0U) && (ez_report_ms < 50U)) ? 50U : ez_report_ms;
    ez_idle_valid = false;
    printf("OK ezreport=%lu\r\n", (unsigned long)ez_report_ms);
}

static void cmd_ezwatch(int argc, char *argv[])
{
    if (argc > 1)
    {
        ez_watch = (0 == strcmp(argv[1], "on"));
        ez_valid = false;
    }
    printf("OK ezwatch=%s bytes=%u\r\n", ez_watch ? "on" : "off", (unsigned)EZ_WATCH_BYTES);
}

/* print every byte of the 4000T buffer that changes (b<index> old->new) */
static void poll_ez(void)
{
    uint8_t b[EZ_WATCH_BYTES];
    if (!ez_watch || (BSP_OK != st_bus) ||
        (BSP_OK != i2c5_read(QWA309_I2C_ADDR_CAPSENSE, b, sizeof(b))))
    {
        return;
    }
    if (ez_valid)
    {
        for (uint32_t i = 0U; i < EZ_WATCH_BYTES; i++)
        {
            if (b[i] != ez_last[i])
            {
                printf("EZCHG b%lu %02X->%02X\r\n", (unsigned long)i, ez_last[i], b[i]);
            }
        }
    }
    (void)memcpy(ez_last, b, sizeof(b));
    ez_valid = true;
}

/* bus speed: 100 kHz default (CapSense), 400 kHz to compare */
static void cmd_i2c5hz(int argc, char *argv[])
{
    uint32_t hz = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 10) : I2C5_DEFAULT_HZ;
    st_bus  = i2c5_init(hz);
    st_caps = (BSP_OK == st_bus) ? capsense_init() : BSP_ERR_INIT;
    printf("OK i2c5hz=%lu bus=%s caps=%s capsfmt=%s\r\n", (unsigned long)hz, bsp_status_str(st_bus),
           bsp_status_str(st_caps), capsense_format_name());
}

static void cmd_reinit(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    st_bus  = i2c5_init(I2C5_DEFAULT_HZ);
    st_caps = (BSP_OK == st_bus) ? capsense_init() : BSP_ERR_INIT;   /* pads untouched */
    print_info();
}

static const console_cmd_t commands[] =
{
    { "bus5",   "bus5                  - SCB5 SCL/SDA levels + bus recovery", cmd_bus5 },
    { "reinit", "reinit                - restart SCB5 + CapSense (after a switch change)", cmd_reinit },
    { "i2c5hz", "i2c5hz 100000|400000 - SCB5 speed + CapSense re-init", cmd_i2c5hz },
    { "ezi2c",  "ezi2c [n]             - raw n bytes (1..64) from the 4000T @0x08", cmd_ezi2c },
    { "ezreport", "ezreport <ms>|0      - periodic FRAME + EZW raw 4000T lines (live view)", cmd_ezreport },
    { "ezwatch","ezwatch on|off       - print every changed 4000T byte (32 B, 50 ms)", cmd_ezwatch },
    { "campins","campins bsp|buttons  - camera RESET/PWDN as BSP, or SW4/SW5 inputs", cmd_campins },
    { "info",   "info                  - app/board info + init status", cmd_info },
    { "pots",   "pots                  - VR1..VR4 raw counts now", cmd_pots },
    { "caps",   "caps                  - CapSense buttons + slider now", cmd_caps },
    { "scan5",  "scan5                 - I2C addresses on SCB5 (3.3 V bus)", cmd_scan5 },
    { "report", "report on|off         - periodic POTS/CAPS lines", cmd_report },
};

int main(void)
{
    board_init();
    console_init(commands, sizeof(commands) / sizeof(commands[0]));
    printf("\r\n[APP] %s %s on %s\r\n", APP_NAME, APP_VERSION, BOARD_NAME);
    board_start_cm55();

    st_pots = pots_init();
    st_bus  = i2c5_init(I2C5_DEFAULT_HZ);
    st_caps = (BSP_OK == st_bus) ? capsense_init() : BSP_ERR_INIT;   /* pads untouched now */
    for (uint32_t i = 0U; i < POTS_COUNT; i++)
    {
        (void)pots_read_raw(i, &pot_last[i]);
    }
    (void)memset(&caps_last, 0, sizeof(caps_last));
    print_info();

    uint32_t next_report = board_millis() + REPORT_PERIOD_MS;
    uint32_t next_poll = board_millis() + POLL_PERIOD_MS;
    for (;;)
    {
        uint32_t now = board_millis();
        if ((int32_t)(now - next_poll) >= 0)
        {
            next_poll = now + POLL_PERIOD_MS;
            poll_inputs();
            poll_ez();
            report_ez(now);
        }
        if (report_on && ((int32_t)(now - next_report) >= 0))
        {
            next_report = now + REPORT_PERIOD_MS;
            print_pots();
            print_caps();
        }
        console_poll();
    }
}

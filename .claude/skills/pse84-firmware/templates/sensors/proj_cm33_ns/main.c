/*******************************************************************************
* File Name : main.c  (CM33 non-secure)
*
* Layer 3 - application. Wires the bsp sensor drivers and the console; no
* CYBSP_* or Cy_* calls here (those belong in board/).
*
* sensors (core scope cm33): reads the AI Kit sensors and prints them on the
* KitProg3 UART every period:
*   SHT40 (humidity/temp) + DPS368 (pressure/temp) + BMI270 (IMU) on SCB0 I2C,
*   BMM350 (magnetometer) on I3C. CM55 is started and stays parked.
* Output (key=value, fixed-point; tests parse it):
*   ENV t=24.81C rh=55.20% p=100812Pa tp=25.40C
*   IMU ax=12mg ay=-8mg az=1003mg amag=1003mg gx=-120mdps gy=30mdps gz=10mdps t=27.12C
*   MAG x=12345nT y=-2345nT z=-40000nT bmag=42000nT t=26.80C
* A sensor that failed init prints "<TAG> ERR <status>" instead of values.
*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "console.h"
#include "led.h"
#include "i2c_scb0.h"
#include "humid_sht4x.h"
#include "baro_dps368.h"
#include "imu_bmi270.h"
#include "mag_bmm350.h"

#define APP_NAME            "sensors"
#define APP_VERSION         "1.0"
#define PERIOD_DEFAULT_MS   (1000U)
#define PERIOD_MIN_MS       (100U)
#define PERIOD_MAX_MS       (10000U)
#define HEARTBEAT_LED       (1U)          /* LED1 toggles on every report */

static uint32_t     period_ms = PERIOD_DEFAULT_MS;
static uint32_t     next_report_ms;       /* board_millis() of the next report */
static bsp_status_t st_bus, st_humid, st_baro, st_imu, st_mag;

/* milli-units -> "-12.34" (two decimals, truncated toward zero) */
static const char *fx(char *buf, size_t len, int32_t milli)
{
    const char *sign = (milli < 0) ? "-" : "";
    uint32_t a = (milli < 0) ? (uint32_t)(-(int64_t)milli) : (uint32_t)milli;
    (void)snprintf(buf, len, "%s%lu.%02lu", sign, (unsigned long)(a / 1000U),
                   (unsigned long)((a % 1000U) / 10U));
    return buf;
}

static uint32_t isqrt64(uint64_t v)
{
    uint64_t r = 0U, bit = (uint64_t)1U << 62;
    while (bit > v)
    {
        bit >>= 2;
    }
    while (0U != bit)
    {
        if (v >= (r + bit))
        {
            v -= r + bit;
            r = (r >> 1) + bit;
        }
        else
        {
            r >>= 1;
        }
        bit >>= 2;
    }
    return (uint32_t)r;
}

static uint32_t mag3(int32_t x, int32_t y, int32_t z)
{
    return isqrt64((uint64_t)((int64_t)x * x) + (uint64_t)((int64_t)y * y) +
                   (uint64_t)((int64_t)z * z));
}

static void report_env(void)
{
    char b1[16], b2[16], b3[16];
    int32_t t = 0, rh = 0, p = 0, tp = 0;
    bsp_status_t s1 = (BSP_OK == st_humid) ? humid_read(HUMID_PREC_HIGH, &t, &rh) : st_humid;
    bsp_status_t s2 = (BSP_OK == st_baro) ? baro_read(&p, &tp) : st_baro;
    if ((BSP_ERR_NODATA == s2) && (0 != p))
    {
        s2 = BSP_OK;      /* faster than the DPS368 rate: report its previous sample */
    }
    if ((BSP_OK == s1) && (BSP_OK == s2))
    {
        printf("ENV t=%sC rh=%s%% p=%ldPa tp=%sC\r\n", fx(b1, sizeof(b1), t),
               fx(b2, sizeof(b2), rh), (long)p, fx(b3, sizeof(b3), tp));
    }
    else
    {
        printf("ENV ERR sht40=%s dps368=%s\r\n", bsp_status_str(s1), bsp_status_str(s2));
    }
}

static void report_imu(void)
{
    char b1[16];
    imu_sample_t s;
    int32_t t = 0;
    bsp_status_t st = (BSP_OK == st_imu) ? imu_read(&s) : st_imu;
    if (BSP_OK == st)
    {
        if (BSP_OK != imu_read_temp(&t))
        {
            t = 0;
        }
        printf("IMU ax=%ldmg ay=%ldmg az=%ldmg amag=%lumg gx=%ldmdps gy=%ldmdps gz=%ldmdps t=%sC\r\n",
               (long)s.ax_mg, (long)s.ay_mg, (long)s.az_mg,
               (unsigned long)mag3(s.ax_mg, s.ay_mg, s.az_mg),
               (long)s.gx_mdps, (long)s.gy_mdps, (long)s.gz_mdps, fx(b1, sizeof(b1), t));
    }
    else
    {
        printf("IMU ERR %s\r\n", bsp_status_str(st));
    }
}

static void report_mag(void)
{
    char b1[16];
    mag_sample_t s;
    bsp_status_t st = (BSP_OK == st_mag) ? mag_read(&s) : st_mag;
    if (BSP_OK == st)
    {
        printf("MAG x=%ldnT y=%ldnT z=%ldnT bmag=%lunT t=%sC\r\n", (long)s.x_nt, (long)s.y_nt,
               (long)s.z_nt, (unsigned long)mag3(s.x_nt, s.y_nt, s.z_nt),
               fx(b1, sizeof(b1), s.temp_mc));
    }
    else
    {
        printf("MAG ERR %s rslt=0x%08lX\r\n", bsp_status_str(st), (unsigned long)mag_last_rslt());
    }
}

static void report_all(void)
{
    report_env();
    report_imu();
    report_mag();
    (void)led_toggle(HEARTBEAT_LED);
}

static void print_status(void)
{
    printf("SENSORS i2c0=%s sht40=%s dps368=%s bmi270=%s bmm350=%s\r\n",
           bsp_status_str(st_bus), bsp_status_str(st_humid), bsp_status_str(st_baro),
           bsp_status_str(st_imu), bsp_status_str(st_mag));
}

static void print_info(void)
{
    printf("INFO app=%s v=%s board=%s scope=cm33 period_ms=%lu\r\n", APP_NAME, APP_VERSION,
           BOARD_NAME, (unsigned long)period_ms);
    print_status();
    printf("READY\r\n");
}

static void sensors_init(void)
{
    st_bus   = i2c0_init(I2C0_DEFAULT_HZ);
    st_humid = (BSP_OK == st_bus) ? humid_init() : BSP_ERR_INIT;
    st_baro  = (BSP_OK == st_bus) ? baro_init(NULL) : BSP_ERR_INIT;
    st_imu   = (BSP_OK == st_bus) ? imu_init(NULL) : BSP_ERR_INIT;
    st_mag   = mag_init();
}

static void cmd_info(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    print_info();
}

static void cmd_read(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    report_all();
    printf("OK read\r\n");
}

static void cmd_rate(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("OK rate period_ms=%lu\r\n", (unsigned long)period_ms);
        return;
    }
    char *end = NULL;
    unsigned long v = strtoul(argv[1], &end, 10);
    if ((NULL == end) || ('\0' != *end) || (v < PERIOD_MIN_MS) || (v > PERIOD_MAX_MS))
    {
        printf("ERR rate %u..%u ms\r\n", (unsigned)PERIOD_MIN_MS, (unsigned)PERIOD_MAX_MS);
        return;
    }
    period_ms      = (uint32_t)v;
    next_report_ms = board_millis() + period_ms;    /* new period starts now */
    printf("OK rate period_ms=%lu\r\n", (unsigned long)period_ms);
}

static void cmd_scan(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    uint8_t found[16];
    if (BSP_OK != st_bus)
    {
        printf("ERR i2c0 %s\r\n", bsp_status_str(st_bus));
        return;
    }
    uint32_t n = i2c0_scan(found, (uint32_t)sizeof(found));
    printf("SCAN bus=i2c0 n=%lu", (unsigned long)n);
    for (uint32_t i = 0U; (i < n) && (i < sizeof(found)); i++)
    {
        printf(" 0x%02X", (unsigned)found[i]);
    }
    printf("\r\n");
}

static void cmd_reinit(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    sensors_init();
    print_status();
}

static void cmd_selftest(int argc, char *argv[])
{
    (void)argc;
    (void)argv;
    bsp_status_t a = (BSP_OK == st_imu) ? imu_selftest() : st_imu;
    bsp_status_t b = (BSP_OK == st_mag) ? mag_selftest() : st_mag;
    printf("SELFTEST bmi270=%s rslt=0x%08lX bmm350=%s rslt=0x%08lX\r\n", bsp_status_str(a),
           (unsigned long)imu_last_rslt(), bsp_status_str(b), (unsigned long)mag_last_rslt());
}

static void cmd_magrate(int argc, char *argv[])
{
    static const uint16_t hz[] = { 12U, 25U, 50U, 100U, 200U, 400U };
    unsigned long f = (argc > 1) ? strtoul(argv[1], NULL, 10) : 0UL;
    unsigned long p = (argc > 2) ? strtoul(argv[2], NULL, 10) : 1UL;
    for (uint32_t i = 0U; i < (sizeof(hz) / sizeof(hz[0])); i++)
    {
        if (hz[i] == f)
        {
            bsp_status_t s = mag_set_rate((mag_odr_t)i, (mag_perf_t)p);
            printf("%s magrate hz=%lu perf=%lu rslt=0x%08lX\r\n", (BSP_OK == s) ? "OK" : "ERR",
                   f, p, (unsigned long)mag_last_rslt());
            return;
        }
    }
    printf("ERR magrate <12|25|50|100|200|400> [perf 0..3]\r\n");
}

static const console_cmd_t commands[] =
{
    { "info",     "info                  - app/board info + sensor status", cmd_info },
    { "read",     "read                  - print all sensors now", cmd_read },
    { "rate",     "rate [ms]             - report period 100..10000 ms", cmd_rate },
    { "scan",     "scan                  - list I2C addresses on SCB0", cmd_scan },
    { "reinit",   "reinit                - re-init bus + sensors", cmd_reinit },
    { "selftest", "selftest              - BMI270 + BMM350 self-test", cmd_selftest },
    { "magrate",  "magrate <hz> [perf]   - BMM350 rate 12..400 Hz, perf 0..3", cmd_magrate },
};

int main(void)
{
    board_init();
    led_init();
    console_init(commands, sizeof(commands) / sizeof(commands[0]));

    printf("\r\n[APP] %s %s on %s\r\n", APP_NAME, APP_VERSION, BOARD_NAME);
    board_start_cm55();
    sensors_init();
    print_info();

    next_report_ms = board_millis() + period_ms;
    for (;;)
    {
        /* signed difference handles the 32-bit tick wrap-around (~49 days) */
        if ((int32_t)(board_millis() - next_report_ms) >= 0)
        {
            next_report_ms = board_millis() + period_ms;
            report_all();
        }
        console_poll();
    }
}

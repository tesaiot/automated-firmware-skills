---
name: psoc-e84-ai-kit
description: Hardware pack for the Infineon PSOC Edge E84 AI Kit (KIT_PSE84_AI) used on its own, board id kit-pse84-ai - on-board component inventory (SW1, LED1/LED2, RGB LED, console UART, BMI270, DPS368, SHT40, BMM350, BGT60TR13C radar, PDM + analog mics, TLV320DAC3100 audio DAC, CYW55513 Wi-Fi/BT, QSPI NOR + octal PSRAM, MIPI-DSI, USB-C camera) with pins, buses and addresses, plus its bsp components and their C APIs. Use together with pse84-firmware when writing, reviewing or porting hardware code for the AI Kit SoM. Base-board (QWA309 / TESAIoT) parts are not here - see qwa309.
---

# PSOC Edge E84 AI Kit - SoM pack

Facts and **bsp components for the AI Kit alone**: everything here must work with no base board.
The workflow (tools, create, build, flash, test, components.py) is in the `pse84-firmware` skill;
this pack only supplies hardware data the engine reads (`pack.json`, `parts.json`, `boards/`,
`bsp_templates/`).

| Item | Where |
| --- | --- |
| board profile | `boards/kit-pse84-ai/board.env` (BSP `KIT_PSE84_AI` 1.4.0, no overlay, `BOARD_PARTS="ai-kit"`) |
| pins, parts, connectors, rework options, verification log | `boards/kit-pse84-ai/README.md` - **the pin source**; the tables below mirror it (if they differ, the board README wins - fix both) |
| parts list for the gates | `parts.json` (group `ai-kit`) |
| bsp components + API headers | `bsp_templates/<id>/component.json` + `files/proj_*/board/*.h`; overview `bsp_templates/README.md` |

Sources: Infineon kit guide 002-41506 Rev *B (2026-04-14), BSP 1.4.0 `design.modus`, Infineon
driver libraries + code examples on github.com/Infineon (checked 2026-10-10).

## On-board components

MCU I/O is **1.8 V** except ports **P16/P17 (3.3 V)**. "Fitted?" = populated on a stock kit.

| Component (ref) | Interface / pins | Addr | Fitted? | Driver lib (Infineon) | bsp component |
| --- | --- | --- | --- | --- | --- |
| PSE846GPS2DBZC4A: CM55 400 MHz + U55 NPU, CM33 200 MHz; RRAM 512 KB, SRAM 1 MB, SoCMEM 5 MB | - | - | yes | BSP / PDL | `ai-kit-board`, `cm55-board` |
| User button SW1 | P7.0, active low (SW2 = RESET) | - | yes | PDL GPIO | `ai-kit-board` (verified) |
| LED1 green / LED2 blue | P10.7 / P10.5, active high | - | yes | PDL GPIO | `ai-kit-board` (verified) |
| RGB LED3 R/G/B | P20.6 / P20.4 / P20.5, active high, TCPWM0 capable | - | yes | PDL TCPWM | `ai-kit-board` (GPIO), `led-rgb-pwm` |
| Console (KitProg3 USB-UART, J1) | SCB2 RX P6.5 / TX P6.7, 115200 8N1 | - | yes | PDL SCB | `ai-kit-board` (verified) |
| Sensor I2C bus (also KitProg3 USB-I2C bridge) | SCB0 SCL P8.0 / SDA P8.1, 1.8 V | - | yes | PDL SCB + mtb_hal_i2c | `i2c-scb0` |
| BMI270 6-axis IMU (U18) | SCB0; INT1 P7.7, INT2 P6.6 | 0x68 | yes | sensor-motion-bmi270 v1 | `imu-bmi270` |
| DPS368 pressure + temp (U6) | SCB0; INT P11.3 | 0x77 | yes | sensor-xensiv-dps3xx v2 | `pressure-dps368` |
| SHT40 humidity + temp (U22) | SCB0 | 0x44 | yes | sensor-humidity-sht4x v1 | `humidity-sht4x` |
| BMM350 magnetometer (U20) | **I3C** SCL P3.0 / SDA P3.1; INT P6.4 | 0x15 | yes | sensor-orientation-bmm350 v1 (`BMM350_I3C`) | `mag-bmm350` |
| BGT60TR13C 60 GHz radar (U5) | SPI SCB3: MISO P21.4, MOSI P21.5, CLK P21.6, CS P21.7; IRQ P20.3; RST P20.7 | - | yes | sensor-xensiv-bgt60trxx v2 (edge iface) | `radar-bgt60` |
| 2x IM73D122 PDM mics (U8 L, U7 R) | PDM0 CLK P8.5, DATA P8.6 (L = ch2 falling, R = ch3 rising) | - | yes | PDL PDM-PCM | `mic-pdm` |
| 2x IM73A135 analog mics (U10 L, U9 R) | CTB P14.0-P14.7 -> SAR | - | yes | PDL autonomous analog | `mic-analog` |
| Audio DAC TLV320DAC3100 (U28) | control SCB0; I2S TX TDM0: MCK P21.3, SCK P21.2, SD P21.1, FSYNC P12.3; speaker **J8** | 0x18 | DAC yes, **J8 no** | audio-codec-tlv320dac3100 v1 | `audio-tlv320dac3100` |
| CYW55513 Wi-Fi 6 + BT 5.4 (Murata 2FY) | SDHC0 SDIO P12.0/P21.0/P12.1/12.2/12.4/12.5, WL_REG_ON P11.4, WL_HOST_WAKE P11.6; BT UART SCB4 P10.0-P10.3, BT_REG_ON P11.0, wake P10.4/P10.6 | - | yes | wifi-core-freertos-lwip-mbedtls v3, btstack | `radio-cyw55513` |
| QSPI NOR 64 MB S25HS512T (U4) | SMIF0 P1.0-P1.3, CS P2.0 (XIP, all images) | - | yes | PDL SMIF / serial-memory | `qspi-flash` (data window) |
| Octal PSRAM 16 MB S70KS1283 (U3) | SMIF1 P4.0-P4.7, CS P0.1, RST P0.0 | - | yes | PDL SMIF | `psram` |
| 3.3 V I2C bus | SCB5 SCL P17.0 / SDA P17.1 -> J10, J16, J7, J14 | - | yes | PDL SCB | `i2c-scb5` |
| MIPI-DSI display (J10, R-Pi 15-pin, 2 lanes) + touch power/I2C (J16) | GFXSS; panel I2C on SCB5 | panel 0x45, FT5406 0x38 | J10 yes, **J16 no**, panel not in kit | display-dsi-waveshare-4-3-lcd, touch-ctp-ft5406 | `display-dsi` |
| USB-C J2 (MCU USB-HS, device/host, DRP U21) + kit USB camera (OV7675 UVC) | USBHS; ID P7.1, DIR P7.3, OUT1/2 P7.5/P7.6, CF P8.3; LED D11 host / D14 device | - | yes (camera in box from Rev *A) | emusb-device / emusb-host | `usb-cdc` (func), `camera-usb-uvc` |
| DVP camera header J14 (kits before Rev *A) | P16.0-7, P17.2-5, P17.7, P11.1 | - | **no** from Rev *A | camera-dvp-ov7675 | - (not planned) |
| Expansion J5 / J17 / QWIIC J7 / SWD J4 / battery J3 | P9.0-3 / P13+P15 / SCB5 / SWD / 3-5 V | - | **no** | - | - |

Address straps and reworks: BMI270 0x69 (R85/R86), DPS368 0x76 (R84), BMM350 to I2C (R220/R223),
boot from RRAM (R187 instead of R188 on P17.6).

## bsp components and their APIs

All APIs return `bsp_status_t` (`board/bsp_status.h`: `BSP_OK`, `BSP_ERR_BUS`, `BSP_ERR_NODEV`,
`BSP_ERR_TIMEOUT`, `BSP_ERR_HW_SETTING`, ...), use integer units (mg, mdps, m°C, Pa, m%RH, nT),
keep the raw vendor result behind `<prefix>_last_rslt()`, never block without a bound, and stay
exec-agnostic (an RTOS registers locks via `i2c0_set_lock()`; IRQ callbacks only set flags).

| Component | Status | Core | Header | Main calls |
| --- | --- | --- | --- | --- |
| `ai-kit-board` | verified | CM33_NS | `board.h`, `bsp_status.h` | `board_init`, `board_start_cm55`, `board_millis`, `board_led_*`, `board_button_*`, `board_console_getc` |
| `cm55-board` | built | CM55 | `board55.h`, `bsp_status.h` | `board55_init`, `board55_led_*`, delay |
| `i2c-scb0` | **verified** | CM33_NS | `i2c_scb0.h` | `i2c0_init(hz)`, `i2c0_write/read/write_read`, `i2c0_reg_read/write`, `i2c0_probe/scan/recover`, `i2c0_set_lock`, `i2c0_hal` |
| `imu-bmi270` | **verified** | CM33_NS | `imu_bmi270.h` | `imu_init(cfg)`, `imu_read(&s)` (mg, mdps), `imu_read_temp`, `imu_selftest`, `imu_set_data_ready_cb` |
| `pressure-dps368` | **verified** | CM33_NS | `baro_dps368.h` | `baro_init(cfg)`, `baro_read(&pa, &mc)`, `baro_ready`, `baro_revision` |
| `humidity-sht4x` | **verified** | CM33_NS | `humid_sht4x.h` | `humid_init`, `humid_read(prec, &mc, &mrh)`, `humid_soft_reset` |
| `mag-bmm350` | **verified** | CM33_NS | `mag_bmm350.h` | `mag_init`, `mag_set_rate(odr, perf)`, `mag_read(&s)` (nT), `mag_selftest` |
| `radar-bgt60` | planned | CM33_NS (CM55 later) | `radar_bgt60.h` | `radar_init(cfg regs)`, `radar_start/stop`, `radar_set_frame_cb`, `radar_read_frame`, `radar_selftest` |
| `mic-pdm` | planned | CM33_NS | `mic_pdm.h` | `mic_init(cfg)`, `mic_start/stop`, `mic_read(pcm, n, &st)`, `mic_set_gain`, `mic_level` |
| `audio-tlv320dac3100` | planned | CM33_NS | `audio_out.h` | `audio_out_init(cfg)`, `audio_out_write(pcm, n)`, `audio_out_set_volume`, `audio_out_mute`, `audio_out_tone` |
| `mic-analog` | planned (research) | CM33_NS | `mic_analog.h` | `amic_init`, `amic_start/stop`, `amic_read`, `amic_level` |
| `qspi-flash` | planned | CM33_NS | `extflash.h` | `extflash_init`, `extflash_read/erase/write` (window-relative), `extflash_xip_ptr`, `extflash_info` |
| `psram` | planned | CM33_NS | `psram.h` | `psram_init`, `psram_base/size`, `psram_test` |
| `radio-cyw55513` | planned | CM33_NS, FreeRTOS | `radio.h` | `radio_wifi_init/connect/scan/ip/rssi`, `radio_bt_init`, `radio_power` |
| `i2c-scb5` | built | CM33_NS | `i2c_scb5.h` | as `i2c-scb0` with prefix `i2c5_`, plus `i2c5_scb_base/context`, `i2c5_irq_enable` |
| `i2c-scb5-cm55` | verified | CM55 | `i2c_scb5.h` | same file as `i2c-scb5`, owned by CM55 (display/touch) |
| `display-dsi` | verified | CM55 (+ CM33_NS clocks) | `display.h`, `display_clocks.h` | `display_init`, `display_gpu_init`, `display_framebuffer(i)`, `display_show`, `display_wait_shown`, `display_set_brightness`, counters; CM33: `display_clocks_enable` before `board_start_cm55` |
| `touch-ft5406` | verified | CM55 | `touch.h` | `touch_init`, `touch_read(&pressed,&x,&y)` (mirrored to display pixels), counters |
| `camera-usb-uvc` | planned (emUSB-Host licence pending) | CM55, FreeRTOS | `camera_uvc.h` | `cam_init(cfg)`, `cam_is_connected`, `cam_start/stop`, `cam_get_frame` |
| `led-rgb-pwm` | planned (blocked by pin claims) | CM33_NS | `rgb_pwm.h` | `rgb_init`, `rgb_set(r,g,b)`, `rgb_set_hex` |

Gate interactions already encoded in the manifests (`components.py check` blocks them):
`i2c-scb5` + `i2c-scb5-cm55` (both claim SCB5: one owner core), `led-rgb-pwm` + `ai-kit-board` (P20.4-6),
`mic-analog` + `qwa309-pots` (SAR), `camera-usb-uvc` + `usb-cdc` (USBHS), `radio-cyw55513` /
`camera-usb-uvc` under baremetal (FreeRTOS only). `hw_settings`: J8 speaker (`audio-tlv320dac3100`),
J16 + power-cycle (`display-dsi`), camera on J2 (`camera-usb-uvc`).

## Quick reference

- **SW1** = user button `CYBSP_USER_BTN1` P7.0, active low; **SW2 = RESET** (the TESAIoT SDK docs
  say "SW2" for the user button - wrong; trust the silkscreen).
- Polarity from the BSP (`CYBSP_LED_STATE_ON`, `CYBSP_BTN_PRESSED`); LED2 has no TCPWM route.
- KitProg3 on USB-C **J1** (J2 = MCU USB); its USB-I2C bridge sits on the sensor bus SCB0.
- Use silkscreen names with users (`BTN_LABELS`), never macro names.

## Rules for components in this pack

- Only on-board parts of the AI Kit; each component lists the pins/peripherals it `claims`
  (components.py blocks two claimers of one resource) and the connectors/reworks it needs in
  `hw_settings`.
- Code that touches QWA309 parts belongs in the `qwa309` pack, never here.
- Implement behind the header in `files/`; wrap the Infineon library (listed in `mtb_deps`) rather
  than re-writing it; TESAIoT SDK source only as a guideline (`guideline` field).
- Status `planned` → `built` (0-warning build) → `verified` (tested on the AI Kit; record it in the
  manifest and the board README verification log).

# bsp_templates - hardware layer of the PSOC Edge E84 AI Kit

Pins, peripherals and device drivers behind small C APIs (`board.h`, `board55.h`, one header per
device). The only layer allowed to use `CYBSP_*` aliases, `Cy_*` / `mtb_hal_*` calls and
`#if defined(BOARD_*)`. Pins come from `../boards/kit-pse84-ai/README.md` / the BSP - never from
the rev1 pinout image. QWA309 base-board drivers live in the `qwa309` pack, not here.
Schema and workflow: `pse84-firmware/reference/components.md`; catalog: `docs/components.md`.

## Layout of a component

```
<id>/component.json              manifest: api, claims, hw_settings, mtb_deps, source, status
<id>/files/proj_cm33_ns/board/   API header (+ .c once implemented) copied into the app
<id>/files/proj_cm55/board/      same, for CM55 components (display-dsi, camera-usb-uvc)
```

Verified 2026-10-10 on TESAIoT (app `sensors`, 17/17): `i2c-scb0`, `humidity-sht4x`,
`pressure-dps368`, `imu-bmi270`, `mag-bmm350` (header + `.c`). A `planned` component ships its **API header only** - the contract the app and func layers code
against. Implementing it = adding the `.c` next to the header, wrapping the Infineon library in
`mtb_deps`, a test app + `tests/*.json`, 0-warning build (→ `built`), hardware test (→ `verified`).

## API conventions (every header)

- Return `bsp_status_t` (`board/bsp_status.h`, shipped by `ai-kit-board` / `cm55-board`);
  `bsp_status_str()` for logs; raw PDL/HAL result via `<prefix>_last_rslt()`.
- `<prefix>_init(const cfg_t *cfg)` with `cfg == NULL` = documented defaults (`*_CONFIG_DEFAULT`),
  `<prefix>_deinit()`; init is re-entrant (re-init after a fault).
- Integer units: acceleration mg, rate mdps, temperature m°C, humidity m%RH, pressure Pa, field nT,
  audio 16-bit PCM interleaved L,R.
- Bounded waits only (timeouts in the header); IRQ callbacks run in IRQ context and only set a
  flag / give a semaphore; `<prefix>_ready()` / `_data_ready()` poll the latched flag.
- Exec-agnostic: no RTOS calls inside; a shared bus takes a lock registered by the exec layer
  (`i2c0_set_lock`). Components needing an RTOS say `exec_models: ["freertos"]`.
- Library handles never leak upward: `i2c0_hal()` etc. are `void *` and for bsp drivers only.

## Components

| Id | Status | Core | Header | Depends on | Claims | hw_settings |
| --- | --- | --- | --- | --- | --- | --- |
| `ai-kit-board` | verified | CM33_NS | `board.h` | - | SCB2, P6.5, P6.7, P7.0, P10.5, P10.7, P20.4-6, SysTick | - |
| `cm55-board` | built | CM55 | `board55.h` | - | - | - |
| `i2c-scb0` | **verified** | CM33_NS | `i2c_scb0.h` | ai-kit-board | SCB0, P8.0, P8.1 | - |
| `imu-bmi270` | **verified** | CM33_NS | `imu_bmi270.h` | i2c-scb0, `sensor-motion-bmi270` | P7.7, P6.6 | - |
| `pressure-dps368` | **verified** | CM33_NS | `baro_dps368.h` | i2c-scb0, `sensor-xensiv-dps3xx` | P11.3 | - |
| `humidity-sht4x` | **verified** | CM33_NS | `humid_sht4x.h` | i2c-scb0, `sensor-humidity-sht4x` | - | - |
| `mag-bmm350` | **verified** | CM33_NS | `mag_bmm350.h` | ai-kit-board, `sensor-orientation-bmm350` | I3C, P3.0, P3.1, P6.4 | - |
| `radar-bgt60` | planned | CM33_NS | `radar_bgt60.h` | ai-kit-board, `sensor-xensiv-bgt60trxx` | SCB3, P21.4-7, P20.3, P20.7 | - |
| `mic-pdm` | planned | CM33_NS | `mic_pdm.h` | ai-kit-board (PDL) | PDM0, P8.5, P8.6 | - |
| `audio-tlv320dac3100` | planned | CM33_NS | `audio_out.h` | i2c-scb0, `audio-codec-tlv320dac3100` | TDM0, P21.1-3, P12.3 | J8 speaker fitted |
| `mic-analog` | planned (research) | CM33_NS | `mic_analog.h` | ai-kit-board (PDL) | SAR, CTB, P14.0-7 | - |
| `qspi-flash` | planned | CM33_NS | `extflash.h` | ai-kit-board (PDL SMIF) | QSPI-DATA-WINDOW | - |
| `psram` | planned | CM33_NS | `psram.h` | ai-kit-board (PDL SMIF) | SMIF1, P4.0-7, P0.1 | - |
| `radio-cyw55513` | planned | CM33_NS (FreeRTOS) | `radio.h` | ai-kit-board, `wifi-core-freertos-lwip-mbedtls` | SDHC0, SCB4, 15 pins | - |
| `i2c-scb5` | planned | CM33_NS | `i2c_scb5.h` | ai-kit-board | SCB5, P17.0, P17.1 | - |
| `display-dsi` | planned | CM55 | `display.h` | cm55-board, `display-dsi-waveshare-4-3-lcd`, `touch-ctp-ft5406` | GFXSS, SCB5 | J16 fitted, power-cycle after flash |
| `camera-usb-uvc` | planned (licence) | CM55 (FreeRTOS) | `camera_uvc.h` | cm55-board, `emusb-host` | USBHS, P7.1/7.3/7.5/7.6, P8.3 | camera on J2 |
| `led-rgb-pwm` | planned (blocked) | CM33_NS | `rgb_pwm.h` | ai-kit-board | TCPWM0-RGB, P20.4-6 | - |

## Suggested implementation order

1. `i2c-scb0` → `humidity-sht4x`, `pressure-dps368`, `imu-bmi270` (one bus, simple polling;
   test app `sensors` printing `ENV t=.. rh=.. p=..` / `IMU ax=..`).
2. `mag-bmm350` (I3C + PREBUILD fix), `mic-pdm` (level meter test), `audio-tlv320dac3100`
   (tone test; needs J8 + speaker).
3. `psram`, `qspi-flash` (pattern tests), `radar-bgt60` (chip id + self-test, then frames).
4. With `freertos-cm33` / `freertos-cm55`: `radio-cyw55513`, `display-dsi`, `camera-usb-uvc`
   (emUSB-Host is binary-EULA: owner decision first).
5. Research / refactor: `mic-analog` (CTB+SAR overlay), `led-rgb-pwm` (ai-kit-board switch).

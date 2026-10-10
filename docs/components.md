# Software components for TESAIoT / KIT_PSE84_AI

Catalog for the bottom-up, AI-assisted workflow: the AI customizes templates into **components**
and integrates them in a thin app layer. Where they live:
- `exec_templates/`, `func_templates/`: engine `.claude/skills/pse84-firmware/` (MCU-generic)
- `bsp_templates/` of the AI Kit: pack `.claude/skills/psoc-e84-ai-kit/` (work on the SoM alone)
- `bsp_templates/` of the base board: pack `.claude/skills/qwa309/` (board `tesaiot` only)

Gates in `components.py`: requires, one exec model per core, pack/board parts, **pin/peripheral
`claims`** (one claimer per resource unless a component `arbitrates` it, e.g. `scb5-lock` for SCB5),
**`hw_settings`** printed as `ACTION: JUMPER` and before flashing. Schema and gates: the skill's `reference/components.md`. Live
list: `python .claude/skills/pse84-firmware/scripts/components.py list --all`.

Research 2026-10-10:
- **Infineon:** deps, Makefiles and READMEs of 33 `mtb-example-psoc-edge-*` code examples (CEs), the BSP
  `release-v1.4.0`, and the MTB manifests.
- **TESAIoT SDK:** source read as a **guideline only**, never built here (Apache-2.0; attribute when
  adapting).
- **Unverified items** are marked [U].

**Status:** verified = tested on our board · built = 0-warning build · planned = manifest only
(the gate refuses it).

## Ground rules from the research

- **Toolchain:** vendor BSP `KIT_PSE84_AI` 1.4.0 (needs tools ≥ 3.7; DSL 1.5.0 pinned), MTB 3.9, GCC 14.2.1. The CEs state
  "MTB 3.6/3.7/3.8 or later". 3.9 meets every minimum but no CE says it was tested [U].
- **Comes with the BSP:** `mtb-ipc` and `mtb-srf` (never add deps lines for them). The BSP owns
  `MTB_IPC_CHAN_0` and the SRF semaphores/IRQs.
- **.mtb line format:** `mtb://<lib>#<tag>#$$ASSET_REPO$$/<lib>/<tag>` (tags `latest-vN.X` or pinned).
- **Core split (Infineon CEs and SDK agree):**
  - **CM33_NS:** console, connectivity (Wi-Fi, BT, USB), storage, SCB0 sensors, OPTIGA, CAN.
  - **CM55:** graphics/LVGL, ML (Ethos-U55), radar ML, PDM audio ML.
  - **CM33_S:** stays the vendor boot shim (TF-M replaces it later).
- **Start-up order rules (SDK guideline):**
  - Start the SAR ADC and enable the GFXSS clocks on CM33 **before** booting CM55, or
    `Cy_GFXSS_Init` fails.
  - Clear the XIP guard before booting CM55.
  - Only CM33 may use printf.
- **FreeRTOS per project:**
  - Add `deps/freertos.mtb`; it pulls in abstraction-rtos and clib-support.
  - Add `COMPONENTS+=FREERTOS RTOS_AWARE` and `DEFINES+=CY_RTOS_AWARE`.
  - Each project needs its own `FreeRTOSConfig.h`: TRUSTZONE 0, MPU 0, FPU 1, 1 kHz tick.
- **Excluded for now:**
  - Voice middleware (audio-voice-core / voice-assistant): LLVM/ARM toolchains only, no GCC.
  - SDK prebuilt `libbento_*.a`: closed source.
  - SDK Imagimob models: All Rights Reserved.
  - SDK `audio_pdm.c`: licence unclear. Use Infineon's pdm-pcm CE instead.
  - emUSB-Host: binary EULA (blocks `camera-usb-uvc` until the owner decides).

## bsp - hardware

| Component | Status | Core | Board parts | Built on (Infineon) | SDK guideline (read, rewrite) |
| --- | --- | --- | --- | --- | --- |
| `ai-kit-board` (board.h, bsp_status.h) | **verified** | CM33_NS | - | BSP aliases | - |
| `qwa309-board` (qwa309.h, SW4/SW5; auto for `tesaiot`) | built | CM33_NS | base-buttons | - | `mod_qwa309_buttons.c` |
| `cm55-board` (board55.h, bsp_status.h) | built | CM55 | - | BSP | - |
| `i2c-scb0` (i2c_scb0.h) sensor bus 1.8 V | **verified** | CM33_NS | sensors | BSP `CYBSP_I2C_CONTROLLER` + `mtb_hal_i2c_setup`, ce:imu-bmi270-freertos | `common/mpy/sensor_i2c.c`: recovery, re-init rules |
| `imu-bmi270` (imu_bmi270.h) @0x68 | **verified** | CM33_NS | sensors | `sensor-motion-bmi270` v1, ce:imu-bmi270-freertos | `sensor_bmi270.c` |
| `humidity-sht4x` (humid_sht4x.h) @0x44 | **verified** | CM33_NS | sensors | `sensor-humidity-sht4x` v1 | `sensor_sht40.c` |
| `pressure-dps368` (baro_dps368.h) @0x77 | **verified** | CM33_NS | sensors | `sensor-xensiv-dps3xx` v2 | `sensor_dps368.c`: temperature fix |
| `mag-bmm350` (mag_bmm350.h) @0x15 I3C | **verified** | CM33_NS | sensors | `sensor-orientation-bmm350` v1 `BMM350_I3C` + bmm350.c patch (components.py patch) | `sensor_bmm350.c`: SETAASA after reset |
| `radar-bgt60` (radar_bgt60.h) | planned | CM33_NS (CM55 later) | radar | `sensor-xensiv-bgt60trxx` v2 edge iface, ce:ml-deepcraft-deploy-radar | `bento_bgt60trxx_platform.c`: bounded SPI wait |
| `mic-pdm` (mic_pdm.h) 2x IM73D122 | planned | CM33_NS | mics | PDL PDM-PCM, ce:pdm-pcm | - (SDK `audio_pdm.c` excluded) |
| `audio-tlv320dac3100` (audio_out.h) @0x18 + I2S TDM0 | planned | CM33_NS | audio | `audio-codec-tlv320dac3100` v1, ce:i2s; needs J8 + speaker | - |
| `mic-analog` (mic_analog.h) 2x IM73A135 | planned (research) | CM33_NS | mics | PDL autonomous analog CTB + SAR, overlay; claims SAR (vs `qwa309-pots`) | - |
| `qspi-flash` (extflash.h) data area, option `flash_area` | planned | CM33_NS | memory | PDL SMIF + BSP memslot, ce:serial-flash-readwrite; areas from CE survey (fs-10mb = littlefs/emFile default) | - |
| `psram` (psram.h) 16 MB octal | planned | CM33_NS | memory | PDL SMIF1, ce:psram-xip | - |
| `radio-cyw55513` (radio.h) Wi-Fi 6 + BT 5.4 | planned | CM33_NS, FreeRTOS | wireless | `wifi-core-freertos-lwip-mbedtls` v3 (+WCM), btstack; ce:wifi-scan, btstack-findme | - |
| `i2c-scb5` (i2c_scb5.h) header/display bus 3.3 V | planned | one owner | - | BSP `CYBSP_I2C_DISPLAY_CONTROLLER` | `ipc_scb5_lock.h` if shared |
| `display-dsi` (display.h) GFXSS + 4.3" DSI + FT5406 | planned | CM55 | display | `display-dsi-waveshare-4-3-lcd`, `touch-ctp-ft5406`, ce:gfx-lvgl-demo (J16) | `gfxss_config.c`, `lv_port_indev.c` |
| `camera-usb-uvc` (camera_uvc.h) kit USB camera on J2 | planned (licence) | CM55, FreeRTOS | camera | `emusb-host` (binary EULA, see exclusions), ce:ml-deepcraft-deploy-vision | - |
| `led-rgb-pwm` (rgb_pwm.h) | planned (blocked by claims) | CM33_NS | leds | BSP `CYBSP_PWM_LED_CTRL` TCPWM0 | - |
| `qwa309-pots` SAR ch4-7 | planned | CM33_NS | pots | PDL AutAnalog + design.modus overlay | `cm55_sensor_poll.c`, `mod_qwa309_pots.c` |
| `qwa309-led-matrix` DFR0522 @0x10 | planned | CM33_NS | led-matrix | - | `dfr0522_rgb.c`: 5-byte cmds, shadow buffer |
| `qwa309-capsense` PSoC 4000T @0x08 | planned | CM33_NS | capsense | - | `capsense_i2c.c`, `capsense_protocol.h` |
| `qwa309-can` CAN FD0 500 kbit/s | planned | CM33_NS | can-rs485 | PDL CANFD | `modcan.c`: bit timing |

AI Kit bsp APIs (headers in each component's `files/`, conventions and order):
`.claude/skills/psoc-e84-ai-kit/bsp_templates/README.md`; inventory: that pack's `SKILL.md`.

## exec - execution model and IPC

| Component | Status | Cores | Notes |
| --- | --- | --- | --- |
| `scope-cm33` / `scope-dual` / `scope-cm55` | **verified** / **verified** / planned | - | core scope: which cores run app code (`--scope`) |
| `baremetal-cm33` | **verified** | CM33_NS | super-loop, `board_millis()` from a 1 ms SysTick |
| `freertos-cm33` | **verified** | CM33_NS | FreeRTOS 10.6, kernel owns SysTick, `board_millis()` = kernel tick (rtos-dual 11/11) |
| `baremetal-cm55` | built | CM55 | super-loop, busy-wait delays |
| `freertos-cm55` | **verified** | CM55 | FreeRTOS 10.6 on the CM55 SysTick, MVE off (rtos-dual 11/11) |
| `ipc-none` | built | - | scope dual with independent cores (no data exchange; E84_TouchUI split) |
| `ipc-mailbox` | **verified** | both | shared-memory mailbox (top of `m33_m55_shared`); first hardware run 2026-10-10 (rtos-dual) |
| `ipc-pipes` | planned | both | PDL `Cy_IPC_Pipe`, ce:ipc-pipes (AI kit listed); SDK guideline `ipc_communication.*`: init semaphores once |
| `mtb-ipc-queue` | planned | both | RTOS-aware queues on a non-BSP channel |
| `scb5-lock` | planned | both | IPC hardware semaphore around SCB5 when both cores use it; quiesce SCB IRQs before unlock |

## func - middleware and custom logic

| Component | Status | Core | Exec | Built on (Infineon) |
| --- | --- | --- | --- | --- |
| `console`, `led`, `button` | **verified** | CM33_NS | baremetal | - |
| `sensor-hub` | planned | CM33_NS | baremetal / freertos | the sensor bsp components; SDK `sensor_auto_task.c` as guideline |
| `storage-littlefs` | planned | CM33_NS | either | `mtb-littlefs` + `serial-memory`, ce:filesystem-littlefs-freertos |
| `usb-cdc` | planned | CM33_NS | either | `emusb-device`, ce:usb-device-hid-mouse |
| `lvgl-ui` | planned | CM55 | freertos | `lvgl` 9.5.0, ce:gfx-lvgl-demo (call LVGL from one task only) |
| `wifi-mqtt` | planned | CM33_NS | freertos | `wifi-core-freertos-lwip-mbedtls`, `mqtt`, ce:wifi-mqtt-client |
| `ml-deepcraft` | planned | CM55 | either | `ml-middleware` / TFLM, ce:ml-deepcraft-deploy-motion. GCC builds fail in RELEASE, so build Debug |

**Later:**
- **BLE:** btstack-integration v7, needs FreeRTOS.
- **OPTIGA Trust M:** v5 needs our own mtb_hal I2C PAL [U].
- **TF-M / MCUboot:** changes proj_cm33_s and the memory layout.
- **Low power:** syspm callbacks, then lpa.
- **Virtual connectivity manager:** Wi-Fi from CM55.

## Suggested bottom-up order

1. **Done:** bring-up (`ai-kit-board`, `baremetal`, `console`, `led`, `button`) is verified on
   TESAIoT.
2. **bsp on bare-metal:**
   1. `i2c-scb0`, then `imu-bmi270`, `humidity-sht4x`, `pressure-dps368`, then `sensor-hub`.
   2. `qwa309-pots` (design.modus overlay).
   3. `i2c-scb5`, then `qwa309-led-matrix` and `qwa309-capsense`.
3. **exec:** run `ipc-mailbox` on the hardware, then `ipc-pipes`. Next `freertos-cm33` (after the
   tick change in `ai-kit-board`), then `freertos-cm55`.
4. **func on FreeRTOS:**
   1. `storage-littlefs`, `usb-cdc`.
   2. `display-dsi`, then `lvgl-ui` (CM55).
   3. `wifi-mqtt`.
   4. `ml-deepcraft`.
5. **Later:** security, low power and BLE.

Each step means: implement the component, `components.py check`, build with 0 warnings
(→ built), test on the board (→ verified), and record the result in the manifest's `verified` field.

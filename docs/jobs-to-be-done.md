# Jobs to be done

Goal: a developer with a TESAIoT Dev Kit (or a bare KIT_PSE84_AI) asks Claude Code for a feature
and gets it built, flashed and tested on the board, with every pin coming from a verified source.

## Done

- 2026-10-10 bring-up on TESAIoT: hello-world, blink, push-to-light built, flashed and tested
  (see the tesaiot board sheet's verification log). VS Code workspaces generated for each app.
- 2026-10-10 core scopes (`baremetal-cm33` / `baremetal-dual` + IPC / `baremetal-cm55` planned) with a
  scope gate; component options gate (`qspi-flash` `flash_area`, default from the Infineon CE survey).
- 2026-10-10 `sensors` app (scope cm33) on TESAIoT: SHT40, DPS368, BMI270, BMM350 drivers verified,
  17/17 UART test PASS; `sensor-debug` skill (PC-side debugging) added and exercised on the board.

## Now - make the base board usable (hardware on the bench)

1. **Resolve the pinout open questions** in
   [hardware/tesaiot-pinout-crosscheck.md](hardware/tesaiot-pinout-crosscheck.md): SW6 = P17.7 (camera
   unplugged), QWA309 SW1/SW2 + DIP SW3/SW4 wiring, VIN, and the SPI SCB. (Done 2026-10-10: rev1
   silkscreen SW5 = P17.5; CAM OFF clamps P17.5/P17.7, CAM ON frees SW5 only.) Get the QWA309 V3_P1
   schematic PDF if possible.
2. **Test SW6 (P17.7)** with the camera unplugged and the CS select switch on → `button-led` PASS on
   all three buttons (SW1, SW5, SW6; `BTN_LABELS` already use the rev1 silkscreen).
3. **Publish a corrected pinout** (rev1 image v2) generated from `boards/tesaiot/README.md`, so the
   image and the skill can't drift apart.

## Now - reuse from E84_TouchUI (<https://github.com/NnopponS/WheelSense_Project/tree/prototype/tesaIoT-hackathon/firmware/E84_TouchUI>)

- [x] 2026-10-10 step 1: library `patches` (Windows-safe, replaces the BMM350 bash PREBUILD),
  BMM350 address fallback, JSON-lines in `sensor-debug` + live view, `reference/troubleshooting.md`,
  `design.modus` tools-version check in `new_app.sh`. Re-tested on TESAIoT: `sensors2` 17/17 PASS.
- [x] 2026-10-10 `ipc-none` for independent cores. [ ] component options carry Makefile keys
  (`CY_IGNORE`, `COMPONENTS`).
- [x] 2026-10-10 FreeRTOS on CM33_NS + CM55 (`freertos-cm33/cm55`, scope/model split, `rtos-dual`
  11/11 on TESAIoT); open issues in `pse84-firmware/reference/exec-models.md` (portable app contract,
  tickless/low power, IPC semaphore, irq-priority check). `web-wizard` skill (project + ask forms).
- [ ] step 3: `freertos-cm55` → `display-dsi` (3 panels via `display_panel` option) + `lvgl-ui`
  (lv_conf, ports, Infineon VGLite overrides, Noto Sans Thai) + `lvgl-app` template (`ui_main(parent)`).
- [ ] step 4: `radio-cyw55513` on either core + `wifi-mqtt`; `ml-tflm-u55` (model `.incbin`, int8
  check script).

## Next - pack hygiene

- Move the SW4/SW5 table entries out of `ai-kit-board`'s board.c into `qwa309-board` (a board-extras
  registration hook), so the SoM pack has no QWA309 code at all; re-verify on hardware.
- `freertos-cm33`: let `ai-kit-board` take `board_millis()` from the RTOS tick (today both claim
  SysTick - the gate blocks the pair on purpose).
- Template pack for the next SoM / base board: copy `psoc-e84-ai-kit` / `qwa309`, edit `pack.json`.

## Next - AI Kit bsp drivers (API headers ready, 2026-10-10)

Headers + manifests in `.claude/skills/psoc-e84-ai-kit/bsp_templates/` (order and conventions in its
README). Each: `.c` wrapping the Infineon library, test app + spec, 0-warning build, hardware test.

1. ~~`i2c-scb0`, `humidity-sht4x`, `pressure-dps368`, `imu-bmi270`, `mag-bmm350`~~ verified 2026-10-10.
   Follow-ups: BMM350 ~2.5 mT board-fixed offset → hard-iron calibration (func) and a bare-AI-Kit
   check (AI Kit or QWA309?); BMI270 data-ready on INT1 (`imu_set_data_ready_cb` returns ERR_PARAM);
   one BMI270 self-test returned ERR_NODEV while the board was handled - re-run still; check whether
   the `bmm350_fix.bash` PREBUILD is still needed with sensor-orientation-bmm350 v1.1 (the library
   now soft-resets + re-runs DAA itself); report the sensor-xensiv-dps3xx wait-loop bug upstream
   (`i < (data_timeout && retry)` never waits).
2. `mic-pdm`, `audio-tlv320dac3100` (DAC answers at 0x18 - read a page-0 register to confirm the
   part; J8 + speaker needed). Then a `sensors` dual-scope variant (CM55 computes tilt/altitude).
3. `psram`, `qspi-flash` (area chosen per app: `--opt flash_area=`), `radar-bgt60`.
4. `baremetal-cm55`: CM33 boot shim, `BOARD_CONSOLE_ON_CM55`, cm55 console + millis.
5. After `freertos-cm33/cm55`: `radio-cyw55513`, `display-dsi` (J16 + panel), `camera-usb-uvc`
   (owner decision on the emUSB-Host binary EULA first).
6. `mic-analog` (CTB+SAR overlay; SAR shared with `qwa309-pots` → needs an arbiter),
   `led-rgb-pwm` (needs a `BOARD_RGB_PWM` switch in `ai-kit-board`).

## Next - QWA309 drivers (M2 board support)

4. (done 2026-10-10) Pots: `qwa309-pots` verified - BSP SAR config re-targeted at run time, no
   overlay needed. Open: full turn reaches only ~1400-1800 counts (sample time vs divider).
5. Header I2C (SCB5): owner core decided - CM33_NS (`i2c-scb5`) in console apps, CM55
   (`i2c-scb5-cm55`) in display/LVGL apps; base-board drivers exist for both cores. Left: build
   `qwa309-led-matrix` (+ the `-cm55` variants) with MTB, test on the second board (0x10 answers),
   promote them from planned; OPTIGA @0x30; `scb5-lock` only if one app needs SCB5 on both cores.
6. On-board sensors on SCB0 (BMI270 0x68, DPS368 0x77, SHT40 0x44): `WHO_AM_I`-style tests.
7. Templates: `pot-read`, `led-matrix`, `sensor-hub` (UART output + `tests/*.json`). Clean-room 2026-11-18: no template offers `cm33 cm33_ns=freertos` with base-board parts (pots + LED matrix + SW5) - add that variant to the first of these; also `scb5-lock` if one app needs SCB5 on both cores.

## Later

8. **Sync with upstream** [automated-firmware-skills](https://github.com/vsupacha/automated-firmware-skills)
   `modus-pdl-edgitalk`: it is newer (open_ide.sh, blink/push-to-light, exit code 10 / `ACTION:`
   contract, milestones.env) but needs the repo-level `lib/` and `boards/`. Decide: vendor those
   in or keep this self-contained copy. Send the board-sheet updates from here back upstream.
   (Upstream also has a stray file `skills/modus-pdl-edgitalk/scripts,\nOUT=` to delete.)
9. Toolchain is fixed: MTB 3.9 + vendor BSP 1.4.0. The TESAIoT SDK (MTB 3.6, BSP 1.2.0) is never
   built here - use its source only as a guideline when writing components.
10. CM55 + NPU path: start from `mtb-example-psoc-edge-ml-*` examples via `examples.sh`; display
    apps need the power-cycle step after flashing.
11. Upstream `push-to-light` test spec has the same `^LED led1=1$` bug (fails on boards with
    more than one LED) - send the fix back.

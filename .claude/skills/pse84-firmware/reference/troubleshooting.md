# Troubleshooting - known pitfalls and their fixes

Symptoms → cause → fix. "Seen here" = reproduced in this repo (date, board); "from E84_TouchUI" =
reported by the TESA team's E84_TouchUI master template, credited below - re-check before relying on
it with our pinned BSP 1.4.0 / ModusToolbox 3.9. Add a row whenever a session finds a new one.

Credit: E84_TouchUI (WheelSense project, TESA team) - `docs/EXTENDING.md` §4 "Common Pitfalls",
<https://github.com/NnopponS/WheelSense_Project/tree/prototype/tesaIoT-hackathon/firmware/E84_TouchUI>
(MIT).

## Toolchain, BSP, build

| Symptom | Cause | Fix | Source |
| --- | --- | --- | --- |
| `new_app.sh` stops: "design.modus was saved with ModusToolbox X but the tools here are Y"; or a build/configurator error naming schema `cydesignfile_v7` | the BSP's `design.modus` was saved by a newer ModusToolbox (newer schema) than the installed tools | install ModusToolbox >= the version in the error, or pin an older BSP; **never** re-save `design.modus` with a newer Device Configurator (it upgrades the schema silently - the base-hash gate then fires) | from E84_TouchUI (§4.6); check in `new_app.sh` |
| `BUILD: BLOCKED (library patch)` / `PATCH: BLOCKED ... pattern not found` | a component's `patches` no longer match the library (new library version) | read the library's release notes; if the upstream fix landed, drop the patch; else update `find_re`/`applied_re` | this repo |
| `PATCH: BLOCKED ... not in mtb_shared` | libraries not fetched yet | `build.sh <app> --getlibs` | this repo |
| duplicate symbol / two `main`-style entry points at link | two files in the app's drop-in folders define the same strong symbol (E84_TouchUI: `example_main`) | keep exactly one; rename stale copies out of compiled folders (`*.c.bak` or move them) | from E84_TouchUI (§4.4) |
| `multiple definition of <asset>` (e.g. `APP_LOGO`) | an app copied a shared asset that a component already provides | delete the app copy; include the component header | from E84_TouchUI (§4.5) |
| `'lv_font_montserrat_XX' undeclared` | LVGL font size not enabled in `lv_conf.h` | enable `LV_FONT_MONTSERRAT_XX` in the LVGL component's `lv_conf.h` (costs 2-20 KB each) | from E84_TouchUI (§4.1) |
| `'CYBSP_I3C_CONTROLLER_HW' undeclared` in app code | app code uses BSP macros directly | layering: move it into a bsp component (`board/`); app code uses the bsp API only | from E84_TouchUI (§4.2), rule here |

## Sensors (see also the `sensor-debug` skill)

| Symptom | Cause | Fix | Source |
| --- | --- | --- | --- |
| BMM350 init OK, every read `ERR_BUS rslt=0xFFFFFFFE` (Bosch -2 COM_FAIL) | `cy_stc_i3c_device_t` passed to `mtb_bmm350_init_i3c()` was a stack variable; the library keeps the pointer and later transfers use a stale dynamic address | make it `static` (done in `mag-bmm350`) | seen here 2026-10-10 tesaiot; E84_TouchUI's driver also keeps it static |
| BMM350 init fails after a soft reset; or reads garbage on Windows builds | Bosch `bmm350_init` soft reset drops the part back to I2C | `mag-bmm350` declares a library patch (`components.py patch`, Windows-safe) - same edit as Infineon's `bmm350_fix.bash` | Infineon library README; E84_TouchUI §4.7 (vendored pre-patched source) |
| BMM350 not found on another board | static address strap differs (0x15 vs 0x14) | `mag_init()` tries 0x15 then 0x14 | idea from E84_TouchUI `bmm350_driver.c` |
| BMM350 magnitude ~2.5 mT, rotation changes it by tens of µT | board-fixed hard-iron offset | calibrate (offset removal) before using the heading | seen here 2026-10-10 tesaiot |
| DPS368 read fails only at fast polling (`ERR_BUS`/`ERR_NODATA` at 200 ms) | `sensor-xensiv-dps3xx` wait loop never waits (`i < (data_timeout && retry)`); data not ready yet | driver returns `BSP_ERR_NODATA` + previous sample (done); poll slower than the DPS rate | seen here 2026-10-10; E84_TouchUI's reader also treats DATA_NOT_READY separately |
| a sensor bus init fails and the whole app stops | init order / one failing bus blocks the rest | best-effort init: report `<bus> init failed (0x...)` and keep the other subsystems running (`SENSORS` line here, `[MASTER] ... unavailable` in E84_TouchUI) | both |
| sensor bus dead after a debugger halt mid-transfer | target holds SDA low | `i2c0_recover()` (9 SCL + STOP) / `reinit` command | this repo |

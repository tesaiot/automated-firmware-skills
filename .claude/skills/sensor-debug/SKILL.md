---
name: sensor-debug
description: PC-side sensor debugging for PSOC Edge E84 / TESAIoT firmware - a Python tool (scripts/sensor_debug.py) drives the running app's console over the KitProg3 UART (no flashing) to check sensor init, I2C scan, self-tests, value plausibility and noise against limits, decode raw driver error codes, sweep rates/settings, and run before/after physical stimulus tests (rotate, flip, breathe, lift), and on request open a browser live preview (scripts/sensor_dash.py + web/index.html template, per-app tests/liveview.json) that charts every reading the board prints and sends console commands. Use when sensor readings are missing, wrong, noisy or failing (ERR_BUS, ERR_NODEV, NODATA, stuck values, odd magnitudes), when verifying a new sensor driver on hardware, or when asked to debug/characterise BMI270, BMM350, DPS368, SHT40 or any sensor printed by an app that follows the pse84-firmware console protocol.
---

# sensor-debug - drive and judge sensor firmware from the PC

The firmware prints numbers; this skill makes the PC decide whether they are right.
`scripts/sensor_debug.py` (Python 3 + pyserial) talks to the **running** app over the KitProg3 UART.
It never flashes, resets or erases - firmware changes go through `pse84-firmware` (build, then
**ask before every flash**). Works with `pse84-firmware/templates/sensors` and any app that keeps
the console protocol below.

```bash
DBG=".claude/skills/sensor-debug/scripts/sensor_debug.py"
python "$DBG" --board tesaiot diag                      # port from apps/.bench/<board>.env (discover.sh)
python "$DBG" --port COMx watch --seconds 10 --limits .claude/skills/sensor-debug/limits/ai-kit-at-rest.json
```

Close other users of the COM port first (serial_test.py, IDE serial monitor, terminal) - only one
program can own it. Gate lines: `DEBUG: PASS|FAIL`, plus `DIAG:`, `CHECK:`, `EXPECT:`, `SWEEP`, `STAT`,
`ERRS` details. Exit 0 = PASS.

## Live preview template (browser, only when the user asks)

Triggers: "show the readings live", "open the live view / dashboard / graph", "watch the sensors in
the browser", "ดูค่าเซนเซอร์แบบสด", "เปิดกราฟ/หน้าเว็บดูค่า". Not for automated checks - those are
`diag` / `watch` / `diff` below.

```bash
LIVE=".claude/skills/sensor-debug/scripts/sensor_dash.py"
python "$LIVE" --app apps/sensors          # board from pse84-app.env, settings from <app>/tests/liveview.json
python "$LIVE" --board tesaiot              # any app; settings from web/config.json
python "$LIVE" --port COMx --config my.json --no-open
```

What it is: serial → `sensor_dash.py` (stdlib HTTP server, 127.0.0.1:8765) → **one** browser page
(`web/index.html`, Server-Sent Events). The server only forwards lines; the page parses the console
protocol and draws one card + chart per `TAG.field`, shows `<TAG> ERR ...` on that sensor group,
keeps a console log, and has a command box + quick buttons (`POST /cmd`). No installs, works
offline, reconnects after USB replug / reflash. A new or reloaded page takes over the stream.

How to run it for the user:
1. Only on request. Start it in the background (`run_in_background: true`); it opens the user's
   default browser (`--no-open` + the built-in browser pane when you need to look yourself).
2. Tell the user the URL, that it holds the COM port, and how to stop it.
3. While it runs, the port is busy: no `sensor_debug.py`, `serial_test.py` or UART test after a
   flash. Commands from you go through it: `curl -s -X POST -d '{"cmd":"rate 200"}'
   http://127.0.0.1:8765/cmd` (the reply appears in the page).
4. Stop it (TaskStop / Ctrl+C) when the user is done or before any other serial tool; say so.
Good pairing: ask for one physical action from the stimulus table while the user watches the
chart, then stop the live view and confirm with `snapshot` / `diff`.

Adapting the template to an app (no code change; the page follows the console protocol):
- Ship `templates/<app>/tests/liveview.json` (copied into the app by `new_app.sh`); `--app` picks it
  up. Keys (defaults in `web/config.json`): `title`, `window_s` (chart length), `quick` (console
  buttons = commands of THAT app), `units` (`{"nT": [0.001, "µT"]}` display conversion), `skip_tags`
  (non-data line tags), `order` (TAG order on the page).
- New data shows up by itself as long as the firmware prints `<TAG> key=<number><unit>` lines;
  failed reads as `<TAG> ERR <status> rslt=0x...`.
- Change `web/index.html` / `sensor_dash.py` only for features every app needs; keep the server a
  dumb forwarder (no storage, one client).

## Commands

| Command | What it does | Use it for |
| --- | --- | --- |
| `ports` | list serial ports | find the KitProg3 COM port |
| `send <cmd>... [--wait S]` | raw console session | anything the app's `help` lists |
| `diag [--seconds N]` | `info` (SENSORS init status), `scan` (I2C addresses), `selftest`, short capture; decodes `rslt=` codes | first step of every session |
| `watch [--seconds N] [--rate MS] [--tags ENV,IMU] [--csv F] [--limits F]` | capture, per-field n/mean/sd/min/max, error counts; limits = range + noise (sd) check | plausibility, noise floor, long runs, fast-rate stress, CSV for plots |
| `snapshot <f.json> [--seconds N]` | save field means | before a physical stimulus |
| `diff <f.json> [--seconds N] [--expect EXPR...]` | means now vs snapshot, deltas, expectations | after the stimulus |
| `sweep --cmd "magrate {v} 1" --values 12,25,100 [--tags MAG]` | run a command per value, count samples/errors | find the setting that breaks it |

`--expect` syntax: `TAG.field>+N` rose >= N, `TAG.field<-N` fell >= N, `TAG.field~N` changed <= N,
`TAG.field!N` changed >= N either way. Units are the ones the firmware prints.
`--log F` (global) appends every received line to a file - keep it next to the app logs.

## Debug playbook (what worked on 2026-10-10)

1. **`diag`** - which sensor fails at init vs at read? Init FAIL `ERR_NODEV` = address/power/rework;
   read `ERR_BUS` after a good init = driver/handle problem, not wiring.
2. **Decode the raw code** (`rslt=`; table `reference/result_codes.json`). Example: BMM350
   `0xFFFFFFFE` = Bosch `-2 E_COM_FAIL` on every transfer after a good init → the I3C device struct
   handed to the library was a stack variable (the library keeps the pointer) - fixed by making it
   static.
3. **`watch --limits`** at the default rate, then **`--rate 200`** (stress). A failure only at the
   faster rate = data-rate / data-ready handling (DPS368: the library's wait loop never waits → NODATA,
   now handled in the driver).
4. **`sweep`** a setting (rate, oversampling, averaging) when one value fails and others work.
5. **Self-test** (`diag` does it) to separate sensor faults from driver faults; keep the board still.
6. **Physical stimulus** - `snapshot`, ask the user to do ONE action (AskUserQuestion, say exactly
   what to do), then `diff --expect ...`:

| Sensor | Ask the user to... | Expect (typical) |
| --- | --- | --- |
| SHT40 humidity | breathe gently on the kit for 3 s | `ENV.rh>+3` `ENV.t>+0.2` |
| DPS368 pressure | lift the board by ~1 m and hold it still | `ENV.p<-8` (about -12 Pa per m) |
| BMI270 accel | turn the board upside down, hold still | `IMU.az<-1500` and `IMU.amag~60` |
| BMI270 gyro | keep still | `watch` sd of `IMU.gx/gy/gz` below the limit |
| BMM350 field | rotate 90° flat on the desk | `MAG.x!8000` or `MAG.y!8000` (Earth ~25-65 uT), `MAG.bmag` ~unchanged if a fixed offset dominates |
| BMM350 field | bring a steel screwdriver close | `MAG.bmag!20000` |

   A delta far smaller than expected = the reading is dominated by an offset or the field is not
   from the sensor axis you think (axis map!). On TESAIoT the BMM350 shows a constant ~2.5 mT
   board-fixed offset (rotation moves x/y/z by only 12-28 uT) - hard-iron calibration needed.
7. **Record evidence**: the result goes into the component manifest (`verified`), the board
   README verification log and, for a new code, `reference/result_codes.json`.

## Console protocol the tool understands (keep it in new apps)

- data `"<TAG> key=<number><unit> ..."` (TAG upper case; units glued to the number: `t=24.81C`,
  `p=101107Pa`, `ax=-12mg`, `x=-345210nT`) - integers or fixed-point, no floats with exponents
- failed read `"<TAG> ERR <bsp status> [rslt=0x%08lX]"` - always print the raw code
- `"SENSORS <name>=<status> ..."` (init), `"SCAN bus=<bus> n=<k> 0x.. ..."`,
  `"SELFTEST <name>=<status> rslt=0x.. ..."`, commands `info`, `scan`, `selftest`, `rate <ms>`,
  `read`, replies `OK ...` / `ERR ...`
- a driver that can't produce fresh data returns `BSP_ERR_NODATA`, never a bus error

## Adding a sensor

1. Driver in its pack's `bsp_templates/` (pse84-firmware components); print it from the app with a
   new TAG following the protocol.
2. Add `TAG.field` limits to a `limits/*.json` (range + `sd_max` + `why` with date/board).
3. Add its stimulus row above and any new result codes to `reference/result_codes.json`.

## Files

| Path | Content |
| --- | --- |
| `scripts/sensor_debug.py` | the tool (stdlib + pyserial) |
| `scripts/sensor_dash.py` + `web/index.html` | live preview template: serial → local server → one browser page (SSE) |
| `web/config.json` | default page settings (title, chart window, quick buttons, units, tag order) |
| `limits/ai-kit-at-rest.json` | AI Kit sensors on a desk: ranges and noise limits |
| `reference/result_codes.json` | status names + raw codes → explanation (extend it) |

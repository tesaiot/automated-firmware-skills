---
name: hw-troubleshooting
description: Hardware troubleshooting for PSOC Edge E84 / TESAIoT Dev Kit (AI Kit + QWA309) boards - find out whether a fault is firmware, configuration, bench setup or hardware, without guessing pins and without risky writes. Covers a stuck/clamped I2C bus (SCB5 SCL/SDA low), devices that NACK (CapSense PSoC 4000T @0x08), CapSense that drifts or never sees buttons (4000T firmware variants frame3/raw5), buttons/switches with unknown pins or no MCU pin (pin watch), comparing against a known-good board or the official SDK firmware (read-only register peek, flash backup/clone, release firmware), a second probe/board on the same PC, and live-view data capture with marked phases. Use when the user says something does not work on the board, "check the HW", "why is X dead", "compare with the working board", "is it firmware or hardware", "which pin is this button", or asks to record findings for the TESA hardware team.
---

# hw-troubleshooting - is it firmware, setup or hardware?

Works with `pse84-firmware` (build/flash), `sensor-debug` (console + live view) and the hardware
packs (`psoc-e84-ai-kit`, `qwa309`). Known TESAIoT findings and their fixes:
[reference/tesaiot-known-issues.md](reference/tesaiot-known-issues.md) - **read it first**, most
symptoms are already there.

## Rules

1. **Least invasive first:** console commands of the running app -> read-only register peek ->
   firmware change. **Ask before every flash** (one approval = one flash, per board). Register
   *writes* through the debugger are not allowed in this setup - add a console command instead
   (e.g. `campins`, `i2c5hz`) and flash that.
2. **One physical action at a time.** The user acts, you record. With the live view running:
   `dash_ctl.py mark <phase>` **before** asking for the action (marks sent after the answer lag the
   action by seconds), keep a clean rest phase between actions, include a **positive control**
   phase (an action that must show a change) - if the control shows nothing, the capture is broken.
3. **Raw data first.** When the live view runs, its bridge owns the port and tees every line into
   `<app>/logs/liveview-*.log`: read that (`dash_ctl.py log`), never stop the view to get data. Show
   firmware data raw (register words, buffer words) before trusting a decoded value.
4. **Separate firmware from hardware** with a reference: the same firmware on a second board, the
   official release firmware on both boards, or a clone of a known-good board. Same firmware,
   different result -> hardware/bench state.
5. **Pins only from the board sheets or the BSP** (never `images/*pinout*.jpg`). A pin found on
   hardware (pin watch) goes into the board sheet's verification log with the date.
6. **Record as you go:** dated line in `.claude/skills/<pack>/boards/<id>/README.md` verification
   log; open questions in `docs/hardware/tesaiot-pinout-crosscheck.md` (remove when answered);
   component `verified`/`notes` fields. Never commit probe serials, COM ports or flash dumps
   (`backup/`, `apps/` are git-ignored).

## Toolbox

| Need | Tool | Notes |
|---|---|---|
| I2C bus levels, recovery, pull-up probe | `bus5` (qwa309-io) | `scl=0 sda=0` with MCU pull-ups on = a device clamps the line |
| re-init / bus speed / camera pins | `reinit`, `i2c5hz 100000\|400000`, `campins bsp\|buttons` | no flash needed once the app has them |
| who answers on SCB5 | `scan5` | TESAIoT: 0x08 CapSense, 0x10 LED matrix, 0x38 touch, 0x45 panel MCU |
| raw 4000T buffer | `ezi2c [n]`, `ezwatch on`, `ezreport <ms>` | `ezreport` feeds the live view (FRAME + EZW words) |
| how does *another* firmware configure pins/peripherals | `scripts/reg_peek.sh <board> [--serial S] scb5 gpio<N> 0xADDR:N` | read-only OpenOCD attach, no reset; decode table in the script header |
| which pin is this button | template `pin-watch` (`new_app.sh <board> <app> "" pin-watch`) | `watch arm` enables plain inputs on reset-default pins only, `PINCHG Pp.n=0/1` |
| what changes when the user does X | live view + `dash_ctl.py mark` + `scripts/phase_diff.py <raw.log> --tag ... --rest '^rest_'` | ignore counters (`--ignore EZW.w1,FRAME.b2`) |
| send commands / read data while the page runs | `sensor-debug/scripts/dash_ctl.py cmd\|mark\|log` | |
| back up a board before overwriting it | `pse84-firmware/scripts/backup.sh <board>` (+ `PSE84_PROBE_SERIAL=` for a second probe) | 12 MB SMIF dump -> `backup/` (acquire resets the chip, read-only) |
| clone / restore / official firmware | `flash.sh <any-app> --yes --board <id> --hex <file>` | check the hex address ranges first; sha256 against the release |
| run a script on the official MicroPython fw | `mpremote connect COMx resume fs cp x.py :x.py` then `mpremote connect COMx resume exec "import x"`; `fs rm :x.py` afterwards | `resume` is required (a soft reset drops back to the friendly REPL + TACP frame -> "could not enter raw repl"); `run x.py` of a ~5 KB script gives random SyntaxError/IndentationError although the bytes arrive intact - import from the fs instead. Ctrl-C via a plain serial write stops it after killing mpremote |
| second board on the same PC | `fw-loader --device-list`, `PSE84_PROBE_SERIAL=<serial>` for flash/backup, `reg_peek.sh --serial` | its UART is another COM port - pass `--port` to the live view |

## Playbooks

**A. Device on SCB5 does not answer / bus stuck low**
1. `bus5`: lines high? -> bus fine, go to B. Low with MCU pull-ups on -> a device clamps it.
2. Toggle one switch at a time (CS select, CAM ON/OFF), wait, re-run `bus5` - a release can take
   minutes (slow supply decay). TESAIoT: CS select ON clamped, OFF released after ~2.5 min.
3. Unpowered devices clamp through their ESD diodes (camera with CAM OFF, a 4000T without supply).
4. Record which switch position frees / clamps the bus.

**B. Device NACKs (e.g. 4000T @0x08 `ERR_NODEV`) on a free bus**
1. `scan5` - is the address there at all? Right after an E84 reset the 4000T may still boot ->
   retry for ~200 ms (`capsense_init` does).
2. Compare with the SDK firmware's settings via `reg_peek.sh` (speed, pin modes). Speed and
   camera pins turned out NOT to matter on TESAIoT - but check, it is cheap.
3. Flash the official release firmware (both MicroPython and C-only) - if it fails the same way,
   it is the board, not our code.

**C. CapSense drifts / buttons never pressed**
1. `ezi2c 64` - is the buffer the 3-byte SDK frame or a larger data block (raw5)? See the known
   issues: raw5 = a 5-segment-slider-only 4000T firmware; byte 2 is a scan counter (the "drift").
2. Map the sensors with isolated touches (mark first; left/middle/right; each button alone).
3. `qwa309-capsense` auto-detects frame3/raw5. Buttons missing on raw5 = needs a 4000T reflash
   (firmware + SWD access from the TESA hardware team) - no E84-side fix.

**D. Button / switch does nothing - find its pin**
1. Flash `pin-watch`, `watch arm`, live view on. Mark, then press each button alone.
2. `PINCHG` names the pin. Nothing at all -> not on a watched MCU GPIO: check the excluded ports
   (header P9.0-3/P13/P15 behind level shifters, P14 mics, P18/P19 crystals), then the I2C devices
   (raw buffer + `phase_diff.py`), then ask for the schematic.
3. Nets shared with other parts read stuck (TESAIoT: SW5/SW6 = camera RESET/PWDN).

**E. Firmware or hardware?**
1. `backup.sh` the known-good board (read-only), build a sparse restore hex of its used areas,
   flash it onto the suspect board -> same symptom = hardware on the suspect board.
2. Official releases (tesaiot-pse84-devkit-sdk GitHub releases): download only with the user's
   OK, verify `SHA256SUMS.txt`, list the hex address ranges (same slots as our apps:
   0x70100000 secure, 0x60340000 CM33, 0x60580000 CM55, 0x60900000) before flashing.
3. A debugger acquire resets the chip but not the base board - a board can lose a "working"
   state across tests; power-cycle (all USB out) before concluding.

## Pitfalls (tooling)

- The live view holds the COM port: use its raw log + `dash_ctl.py`, not a second serial client.
- Bash heredocs in this environment can turn `\r\n` / `\d` into real control characters - write
  helper scripts with the Write tool, or check C `printf` strings after a scripted edit.
- `reg_peek.sh` labels go through Tcl: no `[..]` in labels (the script wraps them in braces).
- Secure aliases (0x5...) of non-secure peripherals read 0 - use 0x4... addresses.
- A board that vanished from `fw-loader --device-list` needs the user: KitProg3 USB-C + data cable.

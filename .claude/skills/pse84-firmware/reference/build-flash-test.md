# Stages 4-6 - Build, flash, test

## Build

- `make build` builds `proj_cm33_s`, `proj_cm33_ns`, `proj_cm55`, signs the secure image with
  Edge Protect Tools (`configs/boot_with_extended_boot.json`) and merges `build/app_combined.hex`.
- `make getlibs` only after changing `deps/*.mtb` or the BSP (build.sh runs it automatically when
  `mtb_shared` is missing, e.g. after `clean.sh --build-only` or copying an app).
  `make build` does **not** rerun the configurators - regenerate after editing `design.modus`.
- build.sh passes `CY_TOOLS_PATHS=<MTB_TOOLS_DIR>` so make uses the same tools check_tools saw.
- First build of a new app ≈ 30 s-3 min; incremental ≈ 10-30 s. The build is reproducible:
  the same sources/tools/libraries give a byte-identical `app_combined.hex` (seen 2026-10-03).
- Outputs: `build/app_combined.hex` (flash this), `build/project_hex/*.elf` (debug/nm).
- Exit codes: 0 PASS, 1 FAIL (make error, stale hex, unsigned), 2 warnings. Manifest only on PASS.
- Quick checks: `arm-none-eabi-nm build/project_hex/proj_cm33_ns.elf | grep board_` (your
  code linked?), `arm-none-eabi-size` for memory.

## Flash

flash.sh (cwd `<app>/proj_cm33_s`, same sequence as `make qprogram` of the recipe):
```
openocd -s <openocd>/scripts -s ../bsps/<BSP>/config/GeneratedSource \
  -c "set QSPI_FLASHLOADER ../bsps/<BSP>/config/GeneratedSource/PSE84_SMIF.FLM" \
  -c "set DEBUG_CERTIFICATE ../packets/debug_token.bin" \
  -c "source [find interface/kitprog3.cfg]; adapter serial <PROBE>; transport select swd; \
      source [find target/infineon/pse84xgxs2.cfg]; adapter speed 12000" \
  -c "gdb_port disabled; telnet_port disabled; tcl_port disabled" \
  -c "init; reset init; flash write_image erase {<hex>}; verify_image {<hex>}; reset run; shutdown"
```
- Before programming, the same command with `init; reset init; shutdown` (acquire only) reads
  `Detected Device` and `Life Cycle` - the read-only attach does not report the life cycle.
- Acquire uses Test Mode (XRES) - it resets the board. Images go to external SMIF flash
  (`0x60000000..`) plus RRAM; OpenOCD pads/erases extra sectors (normal). KitProg3 may print
  "Test Mode acquisition failed" and still continue (seen in RT-Thread logs) - not fatal by itself.
- PASS evidence: `wrote N bytes`, `verified N bytes`, `Detected Device: <MPN>`.
- Fallback: `make qprogram VERBOSE=1` inside modus-shell (doesn't pin the probe serial).
- Ports are disabled so several boards (or an open IDE debug session) don't clash on 3333.

### Recovery

0. Before the first flash: `backup.sh <board>` - `flash read_bank cat1d.cm33.smif1_ns` after
   acquire (a plain `dump_image` returns zeros: SMIF/XIP is not set up in test mode). ~35 s for 12 MB.
1. Re-flash a known-good image: `flash.sh <app> --yes --hex <backup or known-good .hex>`
   (keep one per board, with its sha256, outside `apps/` - clean.sh deletes `apps/`).
2. If attach fails: unplug/replug USB, close other tools using the probe, try another USB
   port/cable, re-run `discover.sh`.
3. Still failing: report to the instructor with the logs. Never change life cycle, provision,
   or erase "everything" to recover.

## Test

Default flow (robust, also after a USB replug):
1. `flash.sh ... --yes` → `FLASH: PASS` (exit 0) or exit 3 → ask the user to replug, wait;
2. `serial_test.py auto <spec> <log> --board <id> [--wait-port 60]` → syncs with `info`/`READY`.

To capture the boot banner instead: start `serial_test.py ... --wait-boot` in the background
first, then flash. Don't combine `--wait-boot` with flash.sh's acquire step and an app that is
already running the same firmware: the old image may answer `READY` before programming.

Interactive steps: tell the user exactly what to press/look at, then run with
`--only-interactive` (after the automatic pass). A human-observed result (LED visibly on) is a
separate verification level - ask and record it.

Notes: the port must not be open in another program (Tera Term, PuTTY, IDE serial monitor).
Events (`EVT ...`) arriving between steps are discarded (input is flushed per step).
Spec keys: docstring of `scripts/serial_test.py`; example: `templates/*/tests/*.json`.

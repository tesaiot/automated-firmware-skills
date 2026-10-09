# Stage 2 - Discover the board, create the project

## discover.sh and the identity gate

`discover.sh <id>` lists probes (`fw-loader --device-list`, pyserial ports with VID 0x04B4 -
the KitProg3 USB-UART's USB serial number equals the probe serial), selects one, and runs
`identity_check.sh`, which executes from `<progtools>/openocd`:
```
openocd -s scripts -c "set ENABLE_ACQUIRE 0" -f interface/kitprog3.cfg \
  -c "adapter serial <PROBE_SERIAL>" -f target/infineon/pse84xgxs2.cfg \
  -c "gdb_port disabled; telnet_port disabled; tcl_port disabled" -c "init; targets; exit"
```
`ENABLE_ACQUIRE 0` skips XRES/test-mode acquisition so the running firmware is not reset.
It prints probe serial, KitProg3 FW, VTarget, `Detected Device`, silicon rev. Life cycle and
boot status are normally NOT printed without acquire - flash.sh checks the life cycle with an
acquire-only attach before programming. On PASS discover.sh writes `<apps>/.bench/<id>.env`.

If attach fails: USB connector (debug port, not the MCU USB port - AI Kit USB-C J1, not J2), VTarget
(~1.8 V on E84 boards), another tool holding the probe (RT-Thread Studio, ModusToolbox IDE,
another OpenOCD), probe mode (must be CMSIS-DAP bulk: `fw-loader --mode kp3-bulk`, ask first),
life cycle (a SECURE/locked part may refuse debug - do not try to "fix" it, report it).

## What new_app.sh does

```
project-creator-cli -b <BSP_ID> --app-path <cached template@commit> -d <workspace> \
    --user-app-name <app> --use-modus-shell
```
- Project Creator clones the BSP **at the latest release in the manifest** and runs getlibs.
  `<workspace>/mtb_shared/` is shared by all apps in that workspace. GitHub must be reachable.
- The BSP folder under `bsps/` is auto-detected (profile `BSP_DIR` only produces a WARN if different).
- `BSP_SOURCE=git:<repo>@<commit>:<subdir>` replaces the vendor BSP with a pinned folder from
  another repo (vendor BSP kept in `<app>/logs/bsp-vendor-<dir>`), then `make getlibs`. Use only
  when a board vendor's BSP differs a lot (TESAIoT's modified `TARGET_KIT_PSE84_AI`); first
  compare it with the stock BSP. An older BSP copied wholesale keeps its old library versions
  and 3.6-era configurator files - prefer porting the deltas (below). Not yet tested on hardware.
- Version gate: `props.json` core version vs `BSP_VERSION` (WARN); sha256 of the base
  `config/design.modus` vs `BSP_BASE_MODUS_SHA256` (STOP when an overlay exists, else WARN).
- Overlays (before regeneration): `BSP_OVERLAY_MODUS` (sha256-checked) replaces
  `config/design.modus` (original kept in `<app>/logs/design.modus.base`); `BSP_OVERLAY_EXTRA`
  "src:dst" pairs copy more files (a `.cyqspi` triggers `qspi-configurator-cli --build`).
  Then `device-configurator-cli --build`.
- Template code is copied over `proj_*`; tests go to `<app>/tests/`; `BOARD_DEFINE` is added to
  `DEFINES+=` in `proj_cm33_ns/Makefile` and `proj_cm55/Makefile` and verified.
- `<app>/pse84-app.env` records board, define, template, BSP version/source, base + overlay
  hashes, template commit, tools.

## Starting from a code example (`--example`)

- `examples.sh` reads Infineon's code-example manifest (`mtb-ce-manifest-fv2.xml`, cached 1 day
  in `~/.cache/pse84-firmware`) and keeps examples whose releases list the BSP's kit capability
  (`kit_pse84_ai`, `kit_pse84_eval_epc2`) and need tools ≤ the installed version. Partner examples
  (Avnet, `kit-pse84-ai-sensor-*`) come from other manifests and are not listed.
- `new_app.sh --example` runs Project Creator in clone mode with app **and** BSP pinned
  (`--app-uri/--app-commit` require `--board-uri/--board-commit`; the BSP is pinned to
  `TARGET_<BSP_ID>@release-v<BSP_VERSION>`, override with `BSP_URI`/`BSP_COMMIT`).
- Many examples ship `templates/TARGET_<kit>/` with their own BSP configuration (e.g.
  btstack-findme enables the Bluetooth parts). Project Creator applies it, so the base
  `design.modus` hash differs from the profile: a WARN on boards without overlay, a STOP on boards
  with an overlay - there, merge the example's BSP changes with the board overlay by
  hand (Device Configurator), then record the result. Verified 2026-10-03: btstack-findme on
  `tesaiot` created + built PASS (not flashed: needs a phone app to test).

## Library drift

The BSP's `deps/*.mtbx` point at `latest-vX.Y`-style versions; getlibs resolves them on the day
you create the app. Example (2026-10-03): `mtb-dsl-pse8xxgp` 1.6.0 → 1.7.0; the only generated
difference on an E84 BSP was `Cy_SysClk_EcoEnable(3000UL)` → `(10000UL)` (longer ECO timeout,
harmless). Always:
1. read the library list in `build/manifest.txt` and compare with the board README log;
2. if a library changed, `diff -r --strip-trailing-cr` the new `GeneratedSource/` against a
   known-good app (ignore header comments) before flashing;
3. record changes in the board README. To pin, edit the `.mtbx`/`.mtb` URL tag and `make getlibs`.

## Porting a BSP delta (new board, or base BSP changed)

1. Start from the vendor BSP matching the exact MPN (`DEVICE` in `bsp.mk`) and EPC category
   (EPC2 vs EPC4 - EPC4 needs another OpenOCD target cfg and OEM keys: stop and ask).
2. Compare the board's reference configuration (vendor/RT-Thread `design.modus`) with the base:
   `python "$SKILL/scripts/modus_diff.py" base.modus board.modus` (semantic diff; many
   `param -> None` lines are only defaults omitted by newer formats).
3. Port only what's needed, in this order: **clocks** (required to boot) → console UART →
   LEDs/buttons → external memory (`design.cyqspi`) → other peripherals → memory map.
4. Edit with Device Configurator (GUI) or carefully in XML, regenerate, diff `GeneratedSource/`,
   check the `CY_CFG_SYSCLK_*` defines against the reference.
5. Save the result as the board overlay, record base + overlay sha256 in `board.env`.

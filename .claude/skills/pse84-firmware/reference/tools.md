# Stage 1 - Tools

## Required set (Windows, proven 2026-10)

| Tool | Proven version | Default location | Used for |
| --- | --- | --- | --- |
| ModusToolbox tools | 3.9.0 | `~/ModusToolbox/tools_3.9` | make, Project Creator, Device Configurator, modus-shell |
| Programming Tools | 1.9.0 | `C:/Infineon/Tools/ModusToolboxProgtools-1.9` | OpenOCD (KitProg3), fw-loader |
| Arm GNU toolchain | 14.2.1 | `~/Infineon/Tools/mtb-gcc-arm-eabi/14.2.1/gcc` | compiler |
| Edge Protect Security Suite | 2.3.0 | `~/Infineon/Tools/ModusToolbox-Edge-Protect-Security-Suite-2.3.0` | signing the secure M33 image (postbuild) |
| Git, Python 3.10+, pyserial | any recent | PATH | template clone, tests |

All come from **ModusToolbox Setup** (Infineon Developer Center) except git/python.
Overrides: set `MTB_TOOLS_DIR`, `PROGTOOLS_DIR`, `PYTHON`, `PSE84_WS` (default apps folder)
before running scripts.

## Folder path rules (checked first by check_tools.sh)

| Path | Rule | If violated |
| --- | --- | --- |
| workspace / app folder | ASCII letters, digits, `-`, `_` only; no spaces | `BAD`; new_app/build/flash refuse to run |
| Windows home (`C:\Users\<name>`) | same | `BAD`: MTB tools, GCC and modus-shell live under it |
| ModusToolbox install | same | `BAD` |
| cloud-synced folder (Dropbox, OneDrive) | allowed | `WARN`: pause sync if getlibs/build hit locked files |

Typical failures: make splits the path at the space (`No rule to make target ...`), configurators
or OpenOCD can't open files whose path has Thai characters, getlibs creates broken library links.
Thai Windows user name → install ModusToolbox outside the home folder and use e.g. `C:/pse84-ws`.

## Version policy

- Profiles record `MTB_TOOLS_VERSION` they were proven with. A mismatch is a WARN, not a stop:
  build once, compare the generated code and library versions, then record the result.
- TESAIoT upstream documents ModusToolbox **3.6 only**. We deliberately use 3.9 (3.6 has
  KitProg/driver problems on lab PCs). Expect configurator-format or library differences. Don't
  import a 3.6-era BSP wholesale (old libraries come with it): start from the current vendor BSP
  and port the deltas (reference/project.md). If you must open a 3.6-era `design.modus` in 3.9,
  let Device Configurator migrate it, then diff the result semantically before using it.
- Several tools_X.Y may coexist; `env.sh` picks the newest unless `MTB_TOOLS_DIR` is set, and
  `common_app.mk` searches `CY_TOOLS_PATHS`. Pin it explicitly in class/CI machines.

## Known failures

| Symptom | Cause | Fix |
| --- | --- | --- |
| `Unable to find any of the available CY_TOOLS_PATHS` | make run outside modus-shell or tools not installed | run via `mtb_make` / modus-shell; install tools |
| `Libraries: "bsp core-make recipe-make" not found` | raw git clone has no BSP | create the app with Project Creator (`new_app.sh`) |
| `No device support library information found` | `device-configurator-cli --library props.json` | omit `--library` |
| no KitProg3 COM port | board on wrong USB connector or driver missing | use the board's debug USB (AI Kit: USB-C J1, not the MCU USB J2); reinstall Programming Tools drivers |

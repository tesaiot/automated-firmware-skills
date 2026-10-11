---
name: pse84-firmware
description: End-to-end firmware workflow engine for Infineon PSOC Edge E84 (PSE84) boards described by hardware packs (psoc-e84-ai-kit SoM, qwa309 base board = TESAIoT DevKit) - with native ModusToolbox and VS Code (ModusToolbox extension) on Windows - check dev tools and folder paths, discover the KitProg3 board, create a Project Creator app with a board BSP overlay, write layered code (board -> func -> main), open it in VS Code, build, flash with identity/life-cycle gates via OpenOCD, run automated UART tests, bring-up demos (hello-world, blink, push-to-light), list and start from Infineon ModusToolbox code examples, and clean the template folder. Use when asked to create, build, flash (แฟลช) or test PSOC Edge / PSE84 / E84 firmware (บอร์ด PSOC Edge), list or start from ModusToolbox examples for E84, port an app to another E84 board, add an E84 board profile, or when the user asks how to use this skill / what commands exist (ขอวิธีใช้, มีคำสั่งอะไรบ้าง, help). Not for PSoC 6 or other MCU families.
---

# PSOC Edge E84 firmware workflow

Six gated stages plus a clean step. Do them in order; do not start a stage until the previous
gate passed. Each stage has a script in `scripts/` and details in `reference/`. Known pitfalls with fixes: `reference/troubleshooting.md` (check it before debugging a
build or sensor failure; add new ones).
**Windows + Git Bash only** (Claude's Bash tool is Git Bash; in PowerShell `bash` may be WSL).

```
1 tools ─► 2 discover + project ─► 3 code ─► 4 build ─► 5 flash ─► 6 test ─► 7 clean
check_tools  discover (board)      board/     build.sh   flash.sh   serial_test  clean.sh
             examples / new_app    func/ main
```

## Approach - bottom-up, components, no TESAIoT SDK builds

Build firmware **bottom-up**: bring-up → bsp components → exec model / IPC → func components →
a thin app layer, each layer proven on the board (gates + tests) before the next goes on top.
Your job is to **customize templates into components and integrate them** (reference/components.md):
reuse a component, else write one, `components.py check` it, wire it in the app template, build,
flash, test.

**Never use the TESAIoT SDK to build firmware**: no bento templates, MicroPython firmware,
SDK BSP 1.2.0, SDK Makefiles/libraries or ModusToolbox 3.6 flow, and never add it as a
dependency. That SDK is top-down (application-first) - a different concept. You MAY read its
source/docs as a guideline when writing a component: rewrite the code into bsp/exec/func layers
on vendor BSP `KIT_PSE84_AI` 1.4.0 + MTB 3.9, put the SDK path in the component's `source` field,
keep its licence terms, and take pins only from `boards/<id>/README.md`.

## Help menu (answer this first when asked how to use the skill)

When the user asks for help, usage, or the list of commands ("ขอวิธีใช้หน่อย", "ใช้ยังไง",
"มีคำสั่งอะไรบ้าง", "help", "what can you do"), run `bash "$SKILL/scripts/help.sh"` (Thai) or
`help.sh --en` and show its output as-is (it lists example requests, the script behind each, and
the boards/templates installed). Then offer the next step that fits (usually Stage 1). Do not run
any other stage just because help was asked.

When the user asks what ModusToolbox examples exist for E84 / a board ("ขอรายการ example",
"มี example bluetooth ไหม"), run `examples.sh <board> [words]` (needs GitHub; cached 1 day) and
show the list; `--detail <id>` for one example. Ask which board if it is not clear - the list
depends on the BSP (`kit-pse84-ai`/`tesaiot` → KIT_PSE84_AI).

`$SKILL` below = the folder containing this SKILL.md. Always quote: `bash "$SKILL/scripts/x.sh"`.
The apps workspace `$PSE84_WS` (default, override by setting it) depends on where the folder sits:

| Skill folder location | Apps workspace | Claude Code finds the skill when |
| --- | --- | --- |
| `<project>/.claude/skills/pse84-firmware` (project skill, e.g. the group template) | `<project>/apps` | a session is opened in `<project>` |
| `~/.claude/skills/pse84-firmware` (user skill) | `./apps` of the current directory | always |
| anywhere else | `./apps` | never - move it to one of the two places above |

Backups (`backup.sh`) go to `$PSE84_BACKUP_DIR` = the workspace's parent `/backup`.
Installing from a copy of just `pse84-firmware/`: put it in one of the two `.claude/skills`
places, keep LF line endings (`.gitattributes` enforces it in git), then Stage 1.

## Hardware packs, board type vs bench instance

This skill is the **engine** (scripts, exec/func components, app templates). Hardware lives in
**packs** - sibling skill folders with a `pack.json`, found by `scripts/packs.py` (`python
"$SKILL/scripts/packs.py"` lists them):

| Pack | Kind | Boards | Holds |
| --- | --- | --- | --- |
| `psoc-e84-ai-kit` | som | `kit-pse84-ai` | AI Kit on its own: pins, on-board parts, bsp components (`ai-kit-board`, `cm55-board`, sensors, radar, display) |
| `qwa309` | baseboard (som `psoc-e84-ai-kit`) | `tesaiot` | QWA309 parts, pin conflicts, switch settings, bsp components (`qwa309-board`, pots, LED matrix, CapSense, CAN) |

Read the pack's `SKILL.md` + `boards/<id>/README.md` before writing hardware code - they are the only
pin source. A new SoM or base board = a new pack folder (copy one, edit `pack.json`/`parts.json`,
add `boards/<id>/`); the engine needs no change. Generic exec/func components stay here.

- **Board type** (`<pack>/boards/<id>/`, same for every copy of that board): `board.env` (BSP
  id/version/base hash, overlay, `BOARD_DEFINE`, `BTN_LABELS`, `BOARD_PARTS` = hardware groups,
  `BOARD_COMPONENTS` = components always applied, expected device + life cycle, OpenOCD cfg),
  `README.md` (pins, quirks, verification log), overlays.
- **Bench instance** (`<workspace>/.bench/<id>.env`, per PC + board, written by `discover.sh`):
  `PROBE_SERIAL`, `CONSOLE_PORT`. Override with env `PSE84_PROBE_SERIAL` / `PSE84_CONSOLE`.
  Never write a probe serial or COM port into a pack.

`BOARD_EXTENDS=<parent>` loads the parent profile first (any pack); the child sets only what differs.
C code sees only `-DBOARD_<ID>`; `board/board.h` maps it to names and extras (`BOARD_HAS_QWA309`).
Unknown board → `boards/_template/README.md` (here). Empty profile values make scripts stop with a
message naming the missing variable - fill them, don't guess. Use the **silkscreen name** with
users (`BTN_LABELS`), never the macro name; say which board a button is on.

## Board-scope, pin-claim and hardware-setting gates

The bare **PSOC Edge E84 AI Kit** (`kit-pse84-ai`) cannot reach the **QWA309 base-board** parts of
the TESAIoT Dev Kit: SW4/SW5, pots VR1..VR4, LED matrix, CapSense, OPTIGA, Arduino/mikroBUS headers,
CAN/RS-485. Only board id `tesaiot` has them (`BOARD_PARTS` in `board.env`; parts, keywords and
source symbols in each pack's `parts.json`). Five checks enforce it - never work around one:

| Where | What | Result |
| --- | --- | --- |
| prompt (project hook `.claude/settings.json` → `board_gate.py hook`) | request names a base-board part **and** targets the bare E84 / AI Kit / `kit-pse84-ai`, not TESAIoT | prompt blocked with the reason (EN + TH); `#board-ok` skips it for discussion. Only the board id `kit-pse84-ai` counts as explicit (not the BSP name `KIT_PSE84_AI`); messages over 1500 chars (agent reports, pasted docs) are not checked |
| before writing code | `python "$SKILL/scripts/board_gate.py" check <board> <parts or words>` | `GATE: PASS` / `GATE: BLOCKED` + `ACTION: CHOOSE` |
| `new_app.sh` | template's `requires.txt` (part ids) vs the board | stops before Project Creator |
| `build.sh` | `board_gate.py app <app>` scans app code outside `board/` (e.g. `GPIO_PRT15`) | `BUILD: BLOCKED (board scope)` |
| compile | `board/qwa309.h` (`#error` unless `BOARD_HAS_QWA309`) - include it from every base-board driver | `BUILD: BLOCKED (board scope, compile time)` |

Two more component rules (`components.py check`, also run by `new_app.sh`):
- **Claims:** components declare the pins/peripherals they `claim`; two claimers of one resource are
  BLOCKED unless a selected component `arbitrates` it (`scb5-lock` for SCB5). A component shipped by
  a pack is BLOCKED on a board without that pack.
- **Hardware settings:** components list `hw_settings` (switches, jumpers, accessories). They print
  as `ACTION: JUMPER`, are stored in `APP_HW_SETTINGS` and shown again by `flash.sh` as `CHECK HW:`.
  Relay them and get the user's OK before tests that depend on them.

## Core-scope and option gates (ask the user, never pick silently)

**Core scope** = which cores run application code; **execution model** = bare metal or FreeRTOS,
chosen **per application core**. Both are the user's choice (`new_app.sh --scope S --model
cm33=..,cm55=..`, or the `web-wizard` project form). Details + known issues: `reference/exec-models.md`.

| Scope (component) | CM33_NS job | CM55 job | Status |
| --- | --- | --- | --- |
| `cm33` (`scope-cm33`) | everything: init, console UART, sensors/peripherals, logic | booted, parked (`idle`) | verified |
| `dual` (`scope-dual` + IPC: `ipc-mailbox` default, `ipc-none` = independent jobs) | init, console, its claimed peripherals, boots CM55, IPC client | compute + its claimed peripherals, IPC worker, no printf | verified |
| `cm55` (`scope-cm55`) | boot shim only (`boot`) | everything incl. console | planned |

Models per core: `baremetal-cm33` (verified) / `freertos-cm33` (verified), `baremetal-cm55`
(built) / `freertos-cm55` (verified). Under FreeRTOS on CM33 the kernel owns SysTick and
`board_millis()` is the kernel tick (0 until the scheduler starts); ISRs that call kernel APIs need
NVIC priority >= 2 (CM33) / >= 1 (CM55).

- Each template lists the variants its `main.c` files implement in `scopes.txt`, one per line:
  `dual cm33_ns=freertos cm55=freertos`. `components.py variant` / `new_app.sh` pick the line
  matching `--scope`/`--model`; one fitting line = used automatically, several = `ACTION: CHOOSE
  variant` - **ask the user** (console questions or the `web-wizard` form), none = refused.
- `components.py` BLOCKS: no/two scopes, an application core without a model, a model or app/driver
  code on a parked core, `dual` without an IPC template (or `ipc-none`), a func/bsp component not
  tested under the chosen model (`exec_models`). `components.py scopes` prints the jobs.
- `pse84-app.env` records `APP_SCOPE` and `APP_MODELS`.

**Several choices at once:** when a new app needs several decisions (board, template, scope,
models, hardware, options, next step), offer the `web-wizard` skill (one browser form, gates
checked live) or console questions - the user picks once per session.

**Library patches:** a component may declare `"patches"` (regex text edits of a vendor library
in `mtb_shared`, e.g. `mag-bmm350` → Bosch `bmm350.c`). `build.sh` applies them after getlibs via
`components.py patch <app>` (idempotent, keeps line endings, no bash PREBUILD - works on Windows).
`PATCH: BLOCKED` = the library changed: re-check the fix, never force it.

**Component options:** a component may need a choice the user must make (`"options"` in its
manifest). Missing → `COMPONENTS: BLOCKED ... needs option <key>` + `ACTION: CHOOSE <key> ...` with
the choices (recommended one marked). Ask, then `new_app.sh ... --opt <key>=<choice>`; the choice
becomes Makefile `DEFINES` and `APP_OPTIONS` in `pse84-app.env`. Example: `qspi-flash` needs
`flash_area` - `fs-10mb` (0x00A00000 + 4 MB, Infineon's file-system default; recommended),
`ce-test-sectors` (0x01F80000 + 512 KB), `top-16mb` (0x03000000 + 16 MB) or
`custom:<offset>,<size>` (256 KB aligned, >= 0x00880000, inside 64 MB); `extflash.h` `#error`s
without it.

On BLOCKED: tell the user which part needs which pack/board and offer a board that has it (or drop
the part). Don't switch the board id on your own, and don't edit `BOARD_PARTS`, `claims` or a
pack to get past a gate. `board_gate.py list <board>` shows what a board has.

## Stage 1 - Tools and folder paths

> **WARNING - folder names.** ModusToolbox (make, configurators, OpenOCD) breaks when a path
> contains **spaces** or **non-English (non-ASCII) characters, e.g. Thai**. This applies to the
> workspace/app folder, the Windows user (home) folder, and the ModusToolbox install folder.
> Use only English letters, digits, `-` and `_`. A Thai Windows user name needs ModusToolbox
> installed and the workspace placed outside the home folder (e.g. `C:/pse84-ws`). Cloud-synced
> folders (Dropbox/OneDrive) work but can lock files during getlibs/build - pause sync if a build
> fails with "permission denied" / "resource busy".

```bash
bash "$SKILL/scripts/check_tools.sh" <board-id> [--fix] [--ws <workspace>]
```
Checks the paths above (`BAD` = must fix; `new_app`, `build`, `flash` also refuse unsafe paths),
then ModusToolbox tools (version vs profile), project-creator/device-configurator CLIs,
modus-shell, Programming Tools (OpenOCD, fw-loader), Arm GCC, Edge Protect Security Suite,
git, python + pyserial, and lists Infineon COM ports. `--fix` only installs pyserial (user pip).
Anything else missing: tell the user what to install (ModusToolbox Setup) - never run installers
needing admin rights yourself. **Gate:** `missing/bad=0`; report every WARN (reference/tools.md).

## Stage 2 - Discover the board, create the project

1. **Discover (needs the board, read-only):** `bash "$SKILL/scripts/discover.sh" <board-id> [--serial <s>]`
   - lists KitProg3 probes + their USB-UART COM ports, picks the only one (or `--serial`),
     runs `identity_check.sh` (attach with `ENABLE_ACQUIRE 0`: no reset/erase; checks probe
     serial + detected device), then saves the bench file.
   - **Gate:** `IDENTITY: PASS` + `Saved ...`. On FAIL stop and report (wrong board/probe, probe
     busy in another tool, locked chip). Re-run after changing board or USB port.
   - No board yet? Skip to step 2 - creating and building need no hardware.
2. **Create the app:** `bash "$SKILL/scripts/new_app.sh" <board-id> <app-name> [<workspace>] [template] [--scope cm33|dual|cm55] [--model cm33=baremetal|freertos[,cm55=...]] [--opt key=value] [--components a,b]`
   - clones the pinned template app, Project Creator with the base BSP (auto-detects the BSP
     folder), optional pinned BSP replacement (`BSP_SOURCE=git:...`), checks BSP version and the
     **base `design.modus` sha256**, applies overlays (hash-checked), regenerates
     `GeneratedSource/`, copies the code template, adds and verifies `-DBOARD_<ID>`.
   - `WARN: ... is in a Dropbox/OneDrive folder` is advice only: pause sync if getlibs/build fail on
     locked files.
   - **Gate:** ends with `Created ...`. Read library versions later in `build/manifest.txt`
     (library drift, reference/project.md).
   - **From a ModusToolbox code example:** `new_app.sh <board-id> <app> [<workspace>] --example <id>`
     (ids from `examples.sh`, prefix `mtb-example-psoc-edge-` optional). Pinned to the example's
     newest release for that BSP (`--example-commit <tag>` to choose); overlays/regeneration/board
     define as usual, but the example's code is kept - no layered template. Before flashing, read
     the example README's hardware needs (radio, display, buttons, phone app...) and compare with
     `boards/<id>/README.md`; write `tests/<app>.json` from the README's expected UART output.
   - Never hand-edit `GeneratedSource/`; edit `design.modus` and regenerate with
     `device-configurator-cli --build <design.modus>` (no `--library`).

### Stage 2d - VS Code (IDE path)

`new_app.sh` ends by preparing the app for **VS Code + Infineon ModusToolbox for VS Code**
(`infineonag.modustoolbox-for-vscode`): `open_ide.sh <app> --no-open` runs ModusToolbox's
`make vscode` (`.vscode/` + `<app>.code-workspace`) and `vscode_fix.py` (the extension's "Fix
Tasks/Settings" applied up front). Open it with `bash "$SKILL/scripts/open_ide.sh" <app>`
(`--refresh` after a BSP/library change). Gate: `IDE: READY <workspace>`; missing VS Code or
extension → tell the user to install it (never install it yourself).

**Two paths after Stage 2** - ask which one when unclear: the developer codes, builds
("Build" task), programs ("Program"/"Build & Program") and debugs ("Multi-Core Debug") in VS Code,
or you run Stages 3-6 with the scripts. Both share the app folder and `build/`; the IDE's
"Program" task has no identity gate. Never overwrite files the developer is editing; on the
script path re-run the gates from Stage 4.

## Stage 3 - Code in layers

Read `reference/layering.md` before writing code. Rules:
- `proj_cm33_ns/board/` - **only** place for `CYBSP_*` aliases, `Cy_*` pin/SCB calls and
  `#if defined(BOARD_xxx)` (e.g. `BOARD_NUM_BUTTONS` when the BSP declares unpopulated I/O).
- `proj_cm33_ns/func/` - reusable services (led, button, console, sensors...). Use `board.h` only.
- `main.c` - wiring + application logic. Uses `func/` (and `board_init`) only.
- Keep `proj_cm33_s` (secure) untouched. Keep the console protocol stable: `OK ...`, `ERR ...`,
  `EVT ...`, `<TAG> k=v ...`, `INFO ... board=<id> leds=N btns=M` then `READY` - tests depend on it.
- Write/extend `tests/<app>.json` from the task requirements before the code; gate board-
  dependent steps with `requires_re` (spec keys: `scripts/serial_test.py` docstring).
- **Components** (reference/components.md): reusable code lives in three layer folders, each
  component = `component.json` + `files/` (mirrors the app tree):
  `bsp_templates/` in the **hardware packs** (`psoc-e84-ai-kit`: `ai-kit-board` = board.h API,
  `cm55-board`; `qwa309`: `qwa309-board` = qwa309.h), `func_templates/` here (services/middleware:
  `led`, `button`, `console`), `exec_templates/` here (core scope, execution models + IPC: `scope-cm33`,
  `scope-dual`, `scope-cm55`, `baremetal-*`, `freertos-*`, `ipc-mailbox`, `ipc-none`). A board's `BOARD_COMPONENTS` are added
  automatically. `components.py list|show|check|apply|scopes` resolves requires, enforces the core
  scope, **one exec model per core**, options, board parts and refuses `planned` components
  (`--allow-planned` only while implementing one). An app template (`templates/<app>/`) holds only
  the application layer (`main.c`, tests) plus `components.txt` (no exec component - the scope adds
  it) and `scopes.txt`; `new_app.sh ... --components a,b` adds more.
  AI Kit sensor drivers (verified 2026-10-10): `i2c-scb0`, `humidity-sht4x`, `pressure-dps368`,
  `imu-bmi270`, `mag-bmm350` - APIs in `psoc-e84-ai-kit/bsp_templates/README.md`.
  To add a capability: pick existing components first; else write a new component (manifest +
  files + README with source/licence), `check` it, then use it from the app template.
- **Bring-up demos** (run them in this order on a new board):
  `hello-world` (UART "hello, world" every 1 s), `blink` (LED1 toggles every 500 ms, prints
  `BLINK led1=x`), `push-to-light` (console `led`/`led?`/`btn?` commands; BTNn press toggles LEDn).
  Others: `button-led` (hold a button → its LED lights; reports silkscreen name + pin; `pins?`
  shows pin config). Board extras not in the BSP (QWA309 buttons) are configured at run time in
  `board.c`.
- `sensors` (scope cm33): SHT40 + DPS368 + BMI270 on SCB0, BMM350 on I3C, printed every 1 s as
  `ENV t=..C rh=..% p=..Pa`, `IMU ax=..mg .. amag=..mg`, `MAG x=..nT .. bmag=..nT`; commands `read`,
  `rate <ms>`, `scan`, `reinit`, `selftest`, `magrate`. Debug it from the PC with the
  `sensor-debug` skill; its live preview (`sensor_dash.py --app <app>`, on request only) charts the
  readings in the browser using the template's `tests/liveview.json`.
- `rtos-dual` (scope dual, FreeRTOS on both cores): CM33 console + blink tasks, CM55 mailbox worker
  task; `tasks`, `blink`, `ping <n>`, `uptime` (both kernel uptimes), `led55`. Verified 11/11.
- `lvgl-ui-app` (scope dual, bare metal on both; display): the UI comes from `ui/ui.json`, generated
  as static C by the `ui-designer` skill (browser editor + this session as the AI designer);
  console `ui`/`uitree`/`uiset`/`uiclick` check it over the mailbox - verified 2026-10-11
- `lvgl-demo` (scope dual, bare metal on both; board tesaiot or a kit with a Waveshare 4.3" DSI):
  CM33 console + `gfx` status over the mailbox, CM55 LVGL 9.5 + VG-Lite + touch (`display-dsi`,
  `touch-ft5406`, `lvgl-ui`) - verified 2026-10-11
- `pin-watch` (diagnostic, any board): button-led + `PINCHG` for any GPIO input change - finds
  the pin of a button that is not in the board sheet (hw-troubleshooting)
- `dual-core-ipc` (scope dual, bare metal on both; CM33 console + CM55 worker over a shared-memory
  mailbox: `ipc?`, `ping <n>`, `led55 <1|2> on|off`; component `ipc-mailbox` adds `<app>/shared`
  and `INCLUDES+=../shared` to both cores). `help.sh` lists templates from `description.txt`.
- Multi-core work (CM55 owns a peripheral, IPC mailbox): reference/layering.md "Dual-core".

## Stage 4 - Build

```bash
bash "$SKILL/scripts/build.sh" <app-dir> [--clean] [--getlibs] [--allow-warnings]
```
Runs `make build -j8` in modus-shell with the pinned tools dir (getlibs automatically if
`mtb_shared` is missing), logs to `<app>/logs/`.
**Gate:** `BUILD: PASS` = make exit 0, fresh `app_combined.hex`, secure image signed, 0 warnings.
Exit 2 = warnings (fix them). `build/manifest.txt` (tools, exact libraries, sha256 of every
artifact) is written **only on PASS** - flash.sh requires it.

## Stage 5 - Flash

**First flash on a board with firmware worth keeping (factory demo):** back it up first -
`bash "$SKILL/scripts/backup.sh" <board-id>` reads the SMIF app area (default 0x60000000 + 12 MB)
through the flash bank into `$PSE84_BACKUP_DIR` (.bin/.hex/.sha256; clean.sh keeps it). Restore with
`flash.sh <app> --yes --hex <backup>.hex`.

**Ask the user before every flash** (board, app, hex sha256 prefix). Then:
```bash
bash "$SKILL/scripts/flash.sh" <app-dir> --yes [--hex <known-good.hex>]
```
Gates inside: app made for this board → hex matches the PASS manifest → read-only identity →
acquire-only attach (resets, writes nothing) with **device + life cycle check** → program,
verify, reset. Never flash a single-core hex; boot order is S-M33 → NS-M33 → M55.
**Gate:** `FLASH: PASS` (`verified N bytes`). Exit 3 = PASS but `ACTION: POWER_CYCLE`: ask the
user to unplug/replug USB, wait for their OK, then test with `--wait-port 60`.

## Stage 6 - Test (after flashing)

```bash
python "$SKILL/scripts/serial_test.py" auto <app>/tests/<spec>.json <app>/logs/test-<ts>.log --board <id> [--interactive | --only-interactive] [--wait-port 60]
```
`auto` = this board's COM port (bench file). Default `--sync` mode sends `info` every second
until `READY` (handles boot time and replugs), checks the running image reports `board=<id>`,
then runs the steps. `--wait-boot` instead listens from before flashing (to capture the banner).
Interactive steps (press a button, look at an LED) run only with `--interactive` /
`--only-interactive`: first tell the user exactly what to do (prompts use `BTN_LABELS`), then run.
**Gate:** `RESULT: PASS`. A human-observed result is a separate level - ask and record it.

## Stage 7 - Clean (reset the template folder)

```bash
bash "$SKILL/scripts/clean.sh" [--build-only] [<workspace>]          # dry run: lists what would go
bash "$SKILL/scripts/clean.sh" [--build-only] [<workspace>] --yes    # delete (ask the user first)
```
- default: deletes every app created by `new_app.sh` (folders with `pse84-app.env`),
  `apps/mtb_shared`, `apps/.cache`, `apps/.bench`; `.claude/` and all other files are kept.
- `--build-only`: keeps app sources/tests/logs; deletes `build/`, `proj_*/libs`, `mtb_shared`,
  cache (rebuild with `build.sh <app> --getlibs`).
- Retries locked files (Dropbox/OneDrive/antivirus); `CLEAN: INCOMPLETE` (exit 1) lists what is
  still locked - pause sync and re-run. Seen on Dropbox 2026-10-03; the second run completed.
- Show the dry-run list and get the user's OK before `--yes`; suggest saving logs/code first.
  Board facts belong in `boards/<id>/README.md` (kept).

## Several boards on one PC

Every board needs its own bench entry: `discover.sh <board-id> --serial <probe>` (with more than
one probe connected, discover refuses to guess). All E84 boards report the same MPN, so the
identity check cannot tell one E84 kit from another: the probe serial is the only link. One
serial may be registered to only one board id (`kit-pse84-ai` and `tesaiot` are the same AI Kit -
pick one). Flash/test then pick the right probe/COM port by board id; `flash.sh` refuses an app
made for another board. Verified 2026-10-03: two E84 boards connected together, hello-world
built, flashed and tested PASS on both from a fresh workspace.

## Sharing the template folder

Everything PC- or board-specific lives under the workspace `apps/` (apps, `mtb_shared`, `.cache`,
`.bench` = probe serials and COM ports) or `backup/` next to it (flash dumps of *your* boards). Before
handing the folder to someone else: `clean.sh --yes`, and move `backup/` out (it is board data,
clean.sh keeps it on purpose). The skill itself holds no user names, absolute paths, serials or
COM ports; tools are located at run time (`env.sh`), and the receiver runs Stage 1 + discover.

## Extending the skill

- **New board / SoM / base board:** a board goes into the pack of its hardware
  (`<pack>/boards/<id>/`, from `boards/_template/README.md` here; fill `board.env` incl. `BOARD_PARTS`
  and `BOARD_COMPONENTS`). A new SoM or base board = a new pack folder with `pack.json` (`kind`,
  `group`, `som` for base boards, `names`/`explicit_ids` for the prompt gate), `parts.json`,
  `SKILL.md` (short, facts only), `boards/`, `bsp_templates/`. Add the `BOARD_<ID>` block to the SoM's
  board component (`psoc-e84-ai-kit/bsp_templates/ai-kit-board/files/proj_cm33_ns/board/board.h`).
- **New component:** `<layer>_templates/<id>/component.json` + `files/` + `README.md`
  (schema and rules: reference/components.md). bsp = only layer with `CYBSP_*`/`Cy_*` pin code;
  func = uses board.h / other func APIs only; exec = loop/RTOS/IPC. Start as `planned`, then
  `built` (0-warning build), then `verified` (tested on hardware) - record where in `verified`.
- **New app template:** `templates/<name>/proj_cm33_ns/main.c` (+ `proj_cm55/main.c` if needed),
  `templates/<name>/tests/<name>.json`, `components.txt` (component ids) and `requires.txt`
  (base-board parts, optional). A template contains only the application layer; `shared/` and
  `ui/` are copied too, and an optional `post_create.sh <appdir>` runs last (env `PYTHON`,
  `SKILLS_DIR`) - e.g. `lvgl-ui-app` generates its UI code there. Names starting
  with `_` are not templates.
- Keep scripts and `.env` files LF; keep everything PC-specific out of the skill (bench files,
  COM ports, serials, user paths) - it must stay copyable.

## Reporting (always)

Report verification level precisely and separately: **source matches → built → flashed →
booted (READY seen) → tested (PASS n/m) → observed by a human**. Never claim a later level
without a log line proving it. Quote hex sha256, tool/library versions, log paths. Add a dated
line to the board's `README.md` verification log when a board-level fact is newly verified.

## Safety rules

- Identity + life-cycle gates before every flash; never flash a board whose serial/device/life
  cycle don't match. Never change life cycle, provision, or write OTP/eFuse.
- Never flash an image built for another board profile or a hex without a PASS manifest
  (except an explicit `--hex` recovery image the user chose).
- Ask before erase/program and before `clean.sh --yes`.
- Don't run vendor installers, driver updates or KitProg3 firmware/mode changes (fw-loader)
  without asking. EPC4 parts or unknown life cycles: stop and ask.

---
name: pse84-firmware
description: End-to-end firmware workflow for Infineon PSOC Edge E84 (PSE84) boards - TESAIoT (TESA AIoT E84) DevKit and Infineon KIT_PSE84_AI - with native ModusToolbox and VS Code (ModusToolbox extension) on Windows - check dev tools and folder paths, discover the KitProg3 board, create a Project Creator app with a board BSP overlay, write layered code (board -> func -> main), open it in VS Code, build, flash with identity/life-cycle gates via OpenOCD, run automated UART tests, bring-up demos (hello-world, blink, push-to-light), list and start from Infineon ModusToolbox code examples, and clean the template folder. Use when asked to create, build, flash (แฟลช) or test PSOC Edge / PSE84 / E84 firmware (บอร์ด PSOC Edge), list or start from ModusToolbox examples for E84, port an app to another E84 board, add an E84 board profile, or when the user asks how to use this skill / what commands exist (ขอวิธีใช้, มีคำสั่งอะไรบ้าง, help). Not for PSoC 6 or other MCU families.
---

# PSOC Edge E84 firmware workflow

Six gated stages plus a clean step. Do them in order; do not start a stage until the previous
gate passed. Each stage has a script in `scripts/` and details in `reference/`.
**Windows + Git Bash only** (Claude's Bash tool is Git Bash; in PowerShell `bash` may be WSL).

```
1 tools ─► 2 discover + project ─► 3 code ─► 4 build ─► 5 flash ─► 6 test ─► 7 clean
check_tools  discover (board)      board/     build.sh   flash.sh   serial_test  clean.sh
             examples / new_app    func/ main
```

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

## Board type vs bench instance

- **Board type** (`boards/<id>/`, part of the skill, same for every copy of that board):
  `board.env` (BSP id/version/base hash, overlay, `BOARD_DEFINE`, `BTN_LABELS`, expected device +
  life cycle, OpenOCD target cfg, quirks), `README.md` (pins, BSP delta, verification log), overlays.
- **Bench instance** (`<workspace>/.bench/<id>.env`, per PC + board, written by `discover.sh`):
  `PROBE_SERIAL`, `CONSOLE_PORT`. Override with env `PSE84_PROBE_SERIAL` / `PSE84_CONSOLE`.
  Never write a probe serial or COM port into `boards/`.

| id | board | base BSP | status |
| --- | --- | --- | --- |
| `kit-pse84-ai` | PSOC Edge E84 AI Kit, on-board I/O only | `KIT_PSE84_AI` 1.4.0, no overlay | verified on HW |
| `tesaiot` | TESAIoT DevKit = AI Kit + QWA309 base board (`BOARD_EXTENDS=kit-pse84-ai`, adds SW4/SW5) | inherited | AI-Kit I/O verified; SW4/SW5 blocked by camera |

`BOARD_EXTENDS=<parent>` in `board.env` loads the parent profile first; the child sets only what
differs (`BOARD_ID`, `BOARD_DEFINE`, `BTN_LABELS`, extras). C code sees only `-DBOARD_<ID>`;
`board/board.h` maps it to names and extras (e.g. `BOARD_HAS_QWA309`).

Unknown board → `boards/_template/README.md`. Empty profile values make scripts stop with a
message naming the missing variable - fill them, don't guess silently.

## Hardware quick reference - switches and LEDs

Use the **silkscreen name** when talking to a user (`BTN_LABELS`), never the macro name.
Polarity always from the BSP: `CYBSP_LED_STATE_ON` (1 on all three), `CYBSP_BTN_PRESSED` (0).

| Board | Switch (silkscreen → pin) | LEDs (→ pin) |
| --- | --- | --- |
| KIT_PSE84_AI (on-board) | **SW1** = user button = `CYBSP_USER_BTN1` (`CYBSP_SW1`) P7[0]; SW2 = RESET. Verified on our kit 2026-10-03 - the TESAIoT SDK docs call the user button "SW2": trust the silkscreen of the board in front of you | LED1 P10[7], LED2 P10[5] (no TCPWM route), RGB red/blue/green P20[6]/P20[5]/P20[4] = `CYBSP_USER_LED3/4/5` (TCPWM0 grp1 lines 265/264/263 → hardware dimming) |
| QWA309 base board (TESAIoT) | **SW4** P17.5, **SW5** P17.7 - not in the BSP: configure at run time (`Cy_GPIO_Pin_FastInit(..., CY_GPIO_DM_PULLUP, 1, HSIOM_SEL_GPIO)`), active low, debounce ≥50 ms. Older docs call them SW9/SW10 or SW5/SW6. **P17.5 is also the USB-host VBUS enable** (buttons and USB joystick are exclusive) and both nets are the DVP camera RESET/PWDN lines: **with the camera plugged in they read 0 permanently** (seen 2026-10-03) - unplug the camera to use SW4/SW5. **QWA309 has a CS select switch that enables its I2C**: with it selected the base-board buttons and CapSense work; without it the TESAIoT base firmware prints `PSOC,ERR,<ms>,I2C,START-W,...` every 100 ms and sees no presses (reported by the lab, 2026-10-03). Check that switch first when base-board I/O is dead. CapSense BTN0/BTN1 + slider = PSoC 4000T @0x08 on the CM55-owned display/touch I2C (P17.0/P17.1) - not GPIO. | No discrete GPIO LEDs. DFR0522 16×8 RGB matrix @0x10 on the header I2C (SCB5, shared with display/touch, owned by CM55) |

QWA309 extras for later groups: pots VR1..VR4 = P15.4..P15.7 (SAR GPIO ch 4..7, knob map
{5,4,6,7} - VR1/VR2 traces swapped), Arduino D2..D9 on P13.x/P15.2-3, header PWM P13.3 (+P13.4
complement), header UART SCB9 P15.0 RX / P15.1 TX, header SPI P9.0-P9.3. Full table with notes:
`boards/tesaiot/README.md`. Source: TESAIoT SDK (schematic QWA309 V3_P1) - **not** the "rev1
pinout" image in the repo's `images/`, which has wrong pins (cross-check in the repo's
`docs/hardware/`). Add-on boards may carry their own "SW1"; always say which board a button is on.

## Board-scope gate - bare E84 AI Kit vs TESAIoT base board

The bare **PSOC Edge E84 AI Kit** (`kit-pse84-ai`) cannot reach the **QWA309 base-board** parts of
the TESAIoT Dev Kit: SW4/SW5, pots VR1..VR4, LED matrix, CapSense, OPTIGA, Arduino/mikroBUS headers,
CAN/RS-485. Only board id `tesaiot` has them (`BOARD_PARTS` in `board.env`; parts, keywords and
source symbols in `boards/components.json`). Five checks enforce it - never work around one:

| Where | What | Result |
| --- | --- | --- |
| prompt (project hook `.claude/settings.json` → `board_gate.py hook`) | request names a base-board part **and** targets the bare E84 / AI Kit / `kit-pse84-ai`, not TESAIoT | prompt blocked with the reason (EN + TH); `#board-ok` in the prompt skips it for discussion |
| before writing code | `python "$SKILL/scripts/board_gate.py" check <board> <parts or words>` | `GATE: PASS` / `GATE: BLOCKED` + `ACTION: CHOOSE` |
| `new_app.sh` | template's `requires.txt` (part ids) vs the board | stops before Project Creator |
| `build.sh` | `board_gate.py app <app>` scans app code outside `board/` (e.g. `GPIO_PRT15`) | `BUILD: BLOCKED (board scope)` |
| compile | `board/qwa309.h` (`#error` unless `BOARD_HAS_QWA309`) - include it from every base-board driver | `BUILD: BLOCKED (board scope, compile time)` |

On BLOCKED: tell the user which part needs the base board and offer to target `tesaiot` (or drop
the part). Don't switch the board id on your own, and don't edit `BOARD_PARTS` to get past the gate.
`board_gate.py list <board>` shows what a board has.

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
2. **Create the app:** `bash "$SKILL/scripts/new_app.sh" <board-id> <app-name> [<workspace>] [template]`
   - clones the pinned template app, Project Creator with the base BSP (auto-detects the BSP
     folder), optional pinned BSP replacement (`BSP_SOURCE=git:...`), checks BSP version and the
     **base `design.modus` sha256**, applies overlays (hash-checked), regenerates
     `GeneratedSource/`, copies the code template, adds and verifies `-DBOARD_<ID>`.
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
- Templates: `templates/_common/` (shared `board/` + `func/` layers, copied into every app) +
  one app template on top. **Bring-up demos** (run them in this order on a new board):
  `hello-world` (UART "hello, world" every 1 s), `blink` (LED1 toggles every 500 ms, prints
  `BLINK led1=x`), `push-to-light` (console `led`/`led?`/`btn?` commands; BTNn press toggles LEDn).
  Others: `button-led` (hold a button → its LED lights; reports silkscreen name + pin; `pins?`
  shows pin config). Board extras not in the BSP (QWA309 buttons) are configured at run time in
  `board.c`.
- `dual-core-ipc` (CM33 console + CM55 worker over a shared-memory
  mailbox: `ipc?`, `ping <n>`, `led55 <1|2> on|off`; template `shared/` is copied to `<app>/shared`
  and `INCLUDES+=../shared` added to both cores). `help.sh` lists templates from `description.txt`.
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

- **New board:** `boards/_template/README.md` (copy to `boards/<id>/`, fill `board.env` incl. `BOARD_PARTS`, add the
  `BOARD_<ID>` block - name, button/LED names, extras - to `templates/_common/proj_cm33_ns/board/board.h`
  and `board.c`). A board built on another one: `BOARD_EXTENDS=<parent>` and only the differences.
- **New app template:** `templates/<name>/proj_cm33_ns/main.c` (+ more files if needed) and
  `templates/<name>/tests/<name>.json`; list needed base-board parts in `requires.txt`. `templates/_common/` (board + func layers) is copied first,
  so a template contains only the application layer. Names starting with `_` are not templates.
- **New service:** add `func/<svc>.c/.h` to `_common` when reusable, else to the app template.
- Keep scripts and `.env` files LF; keep everything PC-specific out of the skill (bench files,
  COM ports, serials, user paths) - it must stay copyable.
- Proven reference not shipped as a template: the dual-core CM33↔CM55 mailbox demo
  (reference/layering.md "Dual-core") - its rules are documented, its code is not included.

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

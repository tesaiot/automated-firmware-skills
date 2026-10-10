# TESAIoT - AI-assisted firmware workflow for PSOC Edge E84

Claude Code skill and hardware notes for the **TESAIoT (TESA AIoT E84) Dev Kit**, which is an
Infineon **PSOC Edge E84 AI Kit** (KIT_PSE84_AI) on the TESA **QWA309** base board. Workflow:
ModusToolbox project → AI-assisted code → build → flash → UART test, with a gate at every stage.
You can switch to VS Code at any point.

Instance of [automated-firmware-skills](https://github.com/vsupacha/automated-firmware-skills),
scoped to this hardware. Poster for the 4 Oct 2026 session: [images/TESAIoT poster Oct 4th.jpg](images/TESAIoT%20poster%20Oct%204th.jpg).

## Approach: bottom-up firmware with AI-assisted components

This repo builds firmware **bottom-up**, one layer at a time, each proven on the board before the
next one goes on top:

1. **Bring-up**: tools, project, build, flash, UART test (hello-world, blink, push-to-light).
2. **bsp**: hardware components (`bsp_templates/`): pins and drivers behind small C APIs.
3. **exec**: execution model and inter-core IPC (`exec_templates/`): bare-metal, FreeRTOS,
   CM33↔CM55.
4. **func**: middleware and custom logic (`func_templates/`): LVGL, sensor processing,
   connectivity, ML.
5. **app**: a thin `main.c` that wires the components, plus a UART test spec.

The AI's job is to **customize code templates into components and integrate them**. It picks or
writes components, checks them (`components.py check`: dependencies, one exec model per core,
board parts), wires them in the app layer, then builds, flashes and tests behind gates.

**The TESAIoT SDK is never used to build firmware.** Its bento templates, MicroPython
firmware, modified BSP 1.2.0 and ModusToolbox 3.6 build are a top-down, application-first design,
a different concept from this repo. Its source code (and its docs) may be read as a **guideline**
when preparing a component template. Rewrite the code into this repo's layers (bsp / exec /
func), use the vendor BSP 1.4.0 + ModusToolbox 3.9, cite the source path in the component's
`source` field and respect its licence. Pins still come only from the board sheets.

## Dev tools

| Tool | Version used | Notes |
| --- | --- | --- |
| ModusToolbox (tools + Project Creator, Device Configurator) | 3.9 | `~/ModusToolbox/tools_3.9` |
| ModusToolbox Programming Tools (OpenOCD, fw-loader) | 1.9 | KitProg3 flashing and debug |
| Arm GNU toolchain | 14.2.1 | installed with ModusToolbox |
| Edge Protect Security Suite | 2.3.0 | signs the secure CM33 image |
| VS Code + **Infineon ModusToolbox for VS Code** (`infineonag.modustoolbox-for-vscode`) | 1.141 / 1.12.0 | the IDE path: build, program and multi-core debug |
| Git for Windows (Git Bash), Python 3 + pyserial | 2.50 / 3.12 / 3.5 | the scripts run in Git Bash |

Stage 1 (`check_tools.sh tesaiot`) checks all of these. Keep the repo path free of spaces and Thai
characters. If it sits in Dropbox, pause sync while getlibs/build run.

## Quick start

1. Open this folder in Claude Code. The project skill `.claude/skills/pse84-firmware` loads
   automatically; ask "ขอวิธีใช้หน่อย" or "how do I use this?" for its command menu.
2. Plug in the kit's **debug USB-C J1** (KitProg3, on the AI Kit), not J2.
3. Ask in your own words, for example: "check my tools for tesaiot", "create a blink app for
   tesaiot and build it", "open it in VS Code", "flash it and run the test". Claude asks before
   every flash.

Bring-up demos (templates), in order: **hello-world** (UART), **blink** (LED1), **push-to-light**
(console LED commands; each button toggles its LED). Generated apps go to `apps/`. That folder is
git-ignored: it holds the projects, `mtb_shared`, and your PC's probe serial and COM port.

## Hardware

Details and sources are in the skill's board sheets:
[kit-pse84-ai](.claude/skills/psoc-e84-ai-kit/boards/kit-pse84-ai/README.md) and
[tesaiot](.claude/skills/qwa309/boards/tesaiot/README.md). Pins come from the vendor
BSP `KIT_PSE84_AI` 1.4.0, the Infineon kit guide and the TESAIoT SDK docs/source as a reference
(they quote schematic QWA309 V3_P1).

### PSOC Edge E84 AI Kit (the SoM)

| Item | Value |
| --- | --- |
| MCU | `PSE846GPS2DBZC4A` (E84, EPC2): Cortex-M55 400 MHz + Ethos-U55 NPU, Cortex-M33 200 MHz (secure + non-secure) |
| Memory | RRAM 512 KB, SRAM 1 MB, SoCMEM 5 MB; QSPI NOR 512 Mb S25HS512T (XIP 0x60000000); octal PSRAM 128 Mb S70KS1283 |
| I/O voltage | **1.8 V** on all ports except P16/P17 (3.3 V) |
| Debug | KitProg3 on **USB-C J1**: SWD + USB-UART console (SCB2, P6.5 RX / P6.7 TX, 115200 8N1). MCU USB-HS is on J2 |
| User I/O | **SW1** user button P7.0 (active low), **SW2** = reset; LED1 green P10.7, LED2 blue P10.5, RGB LED red/blue/green P20.6/P20.5/P20.4 (active high) |
| Sensors | BMI270 IMU (I2C 0x68), DPS368 pressure (0x77), SHT40 temp/humidity (0x44) on SCB0 P8.0/P8.1 (1.8 V); BMM350 magnetometer on I3C P3.0/P3.1 |
| Radar / audio | BGT60TR13C 60 GHz radar (SPI SCB3, P21.4-P21.7); 2 PDM mics (P8.5/P8.6); 2 analog mics (P14.x); TLV320DAC3100 audio DAC @0x18 (I2S TDM0), speaker connector J8 not fitted |
| Wireless | CYW55513 Wi-Fi 6 + Bluetooth 5.4 (SDIO + HCI UART SCB4) |
| Display / camera | MIPI-DSI J10 (R-Pi 15-pin; touch/5 V header J16 not fitted); 3.3 V I2C SCB5 P17.0 SCL / P17.1 SDA; camera = 0.3 MP USB (UVC) on J2 from kit Rev *A (DVP header J14 only before Rev *A, nets P16.x/P17.x) |

### QWA309 base board (TESAIoT)

| Item | Value |
| --- | --- |
| Buttons | SW4/SW5 (rev1 silkscreen may read SW5/SW6) on P17.5/P17.7, active low with pull-up. **These are the DVP camera RESET/PWDN nets: with the camera fitted they read "pressed"** |
| Pots VR1..VR4 | P15.4..P15.7 (SAR ch 4..7, 0-1.8 V); the knobs labelled VR1 and VR2 are swapped (VR1 = P15.5) |
| Arduino header | D0/D1 UART SCB9 P15.0/P15.1 · D2 P13.0 · D3 P13.5 · D4 P13.6 · D5 P13.4 · D6 P13.3 · D7 P13.7 · D8 P15.3 · D9 P15.2 · D10-D13 SPI P9.0 CS / P9.2 MOSI / P9.1 MISO / P9.3 SCK · A4/A5 = I2C P17.1/P17.0. A0-A3, VIN, AREF not connected |
| On the header I2C (SCB5, 3.3 V) | CapSense buttons + slider (PSoC 4000T @0x08), DFRobot 16×8 RGB LED matrix (@0x10), OPTIGA Trust M (@0x30), mikroBUS ×3, QWIIC |
| Other | CAN transceiver (P16.2/P16.3), RS-485, CS select switch (must be ON for base-board I2C), CAM ON/OFF switch |

> **Board-scope gate:** the base-board parts above exist only on the TESAIoT Dev Kit. A request
> that asks for them on the bare PSOC Edge E84 AI Kit is blocked when you send it (project hook in
> `.claude/settings.json`), and again when an app is created, built or compiled. Name the board
> `tesaiot` instead, or add `#board-ok` to a prompt that only discusses it. Details: the skill's
> `SKILL.md`, section "Board-scope gate".

> The [rev1 pinout image](images/TESAIoT%20Dev%20Kit%20rev1%20pinout.jpg) has wrong pins (pots,
> D0-D9, buttons, A0-A5). Use the tables above. Row-by-row check:
> [docs/hardware/tesaiot-pinout-crosscheck.md](docs/hardware/tesaiot-pinout-crosscheck.md).

### Links

- TESAIoT SDK (reference only, never built here; see Approach): <https://github.com/tesaiot/tesaiot-pse84-devkit-sdk> · docs: <https://tesaiot.github.io/tesaiot-pse84-devkit-sdk/>
- Infineon KIT_PSE84_AI kit guide: <https://www.infineon.com/assets/row/public/documents/30/44/infineon-kit-pse84-ai-user-guide-usermanual-en.pdf>
- BSP: <https://github.com/Infineon/TARGET_KIT_PSE84_AI> · code examples: <https://github.com/orgs/Infineon/repositories?q=mtb-example-psoc-edge>

## Where things are

| Path | What | Who reads it |
| --- | --- | --- |
| [.claude/skills/pse84-firmware/](.claude/skills/pse84-firmware/SKILL.md) | **engine** skill: scripts, `exec_templates/` (core scopes cm33 / dual / cm55 + IPC), `func_templates/`, app `templates/` (incl. `sensors`), references | the agent (and you, for manual runs) |
| [.claude/skills/web-wizard/](.claude/skills/web-wizard/SKILL.md) | **browser forms** instead of one-by-one console questions: new-app wizard (board, template, scope + bare metal/FreeRTOS per core, IPC, hardware, options, next step - gates checked live) and a generic question form | the agent when several choices are pending |
| [.claude/skills/sensor-debug/](.claude/skills/sensor-debug/SKILL.md) | **PC-side sensor debugging**: `sensor_debug.py` drives the running app's console over the KitProg3 UART (diag, limits, sweeps, before/after stimulus tests, error-code table) and, on request, a live browser view of the readings (`sensor_dash.py`); never flashes | the agent while debugging / verifying sensors |
| [.claude/skills/psoc-e84-ai-kit/](.claude/skills/psoc-e84-ai-kit/SKILL.md) | **SoM pack**: AI Kit on its own - board `kit-pse84-ai`, pins, `parts.json`, `bsp_templates/` | the agent, for AI Kit hardware code |
| [.claude/skills/qwa309/](.claude/skills/qwa309/SKILL.md) | **base-board pack**: QWA309 - board `tesaiot`, base-board parts, pin conflicts, switch settings, `bsp_templates/` | the agent, for base-board hardware code |
| `.claude/skills/<pack>/boards/<id>/` | **verified** board facts: `board.env` + `README.md` (pins, quirks, verification log) | the agent while writing `board.c` and flashing |
| [docs/components.md](docs/components.md) | component catalog: implemented, planned, bring-up order | people + the agent when composing |
| [docs/hardware/](docs/hardware/README.md) | hardware research: sources, pinout-image cross-check, open questions | people; the agent only when asked to verify something |
| [docs/jobs-to-be-done.md](docs/jobs-to-be-done.md) | backlog | maintainers |
| `images/` | pinout diagram (rev1, see warning) and poster | people |
| `apps/`, `keepout/` | local only, never committed | - |

A future SoM or base board = one more pack folder next to these; the engine finds it through
`pack.json` and needs no change.

**Why board facts live in the skill and the cross-check lives in docs:** the agent writes pin code
from whatever the skill tells it, so the skill holds only facts proven by the BSP, the schematic or
a test on the board. Unresolved claims stay in `docs/` so the agent can't generate code against a
wrong pin. Once an item is verified, it moves into the skill's board sheet.

## Acknowledgements

- **E84_TouchUI** (WheelSense project, TESA team, MIT) - <https://github.com/NnopponS/WheelSense_Project/tree/prototype/tesaIoT-hackathon/firmware/E84_TouchUI>. Ideas and patterns reused here,
  rewritten into this repo's components (never built on top of it): Windows-safe library patching
  instead of bash PREBUILD, BMM350 address fallback, JSON-lines telemetry support in `sensor-debug`,
  the pitfalls list in `pse84-firmware/reference/troubleshooting.md`. Planned next from the same
  project: display/LVGL stack, Wi-Fi + MQTT, TFLM on Ethos-U55 (see `docs/jobs-to-be-done.md`).
- Infineon ModusToolbox code examples and driver libraries (cited per component in `source`).

## License

MIT for the skill (see its source repo). Vendor files fetched into `apps/` keep their own licenses.

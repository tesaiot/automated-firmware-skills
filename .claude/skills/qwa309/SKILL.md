---
name: qwa309
description: Hardware pack for the TESAIoT QWA309 base board (TESAIoT Dev Kit = PSOC Edge E84 AI Kit on QWA309, board id tesaiot) - base-board parts (SW4/SW5, pots VR1..VR4, DFR0522 LED matrix, CapSense, Arduino/mikroBUS headers, CAN/RS-485, OPTIGA), their pin conflicts and the switch settings they need (CS select, CAM ON/OFF, CAN/RS-485). Use together with pse84-firmware when code or a request touches base-board parts, or when base-board I/O misbehaves. Not usable on the bare AI Kit (kit-pse84-ai).
---

# QWA309 base board - TESAIoT Dev Kit pack

Adds the QWA309 base board to the `psoc-e84-ai-kit` SoM pack (`pack.json`: kind `baseboard`,
som `psoc-e84-ai-kit`). Board `tesaiot` = `BOARD_EXTENDS=kit-pse84-ai` + `BOARD_PARTS="ai-kit qwa309"`
+ `BOARD_COMPONENTS="qwa309-board"`. The workflow is in `pse84-firmware`.

| Item | Where |
| --- | --- |
| board profile | `boards/tesaiot/board.env` |
| pins, Arduino map, quirks, verification log | `boards/tesaiot/README.md` - the only pin source (never the rev1 pinout image) |
| parts + prompt keywords + source symbols | `parts.json` (group `qwa309`) |
| bsp components | `bsp_templates/`: `qwa309-board` (qwa309.h + SW4/SW5, built), planned `qwa309-pots`, `qwa309-led-matrix`, `qwa309-capsense`, `qwa309-can` |

## Gates (enforced by the engine - relay them, never work around them)

- **Board scope:** every component in this pack (and every part in `parts.json`) is BLOCKED for a
  board without group `qwa309` - prompt hook, `components.py check`, `new_app.sh`, `build.sh` and
  the `#error` in `board/qwa309.h`.
- **Pin conflicts:** components `claim` resources; two claimers of one resource are BLOCKED unless
  a selected component `arbitrates` it (e.g. `scb5-lock` for SCB5 used by CM33 and CM55).
- **Hardware settings:** components list `hw_settings`; `components.py` prints them as
  `ACTION: JUMPER`, `new_app.sh` stores them as `APP_HW_SETTINGS`, `flash.sh` prints `CHECK HW:`
  before programming. Ask the user to confirm them before tests that use the part.

## Known conflicts and settings

| Part | Pins | Conflict / setting |
| --- | --- | --- |
| SW4 / SW5 | P17.5 / P17.7, active low, pull-up | = DVP camera RESET/PWDN: **camera unplugged or CAM OFF**, else they read pressed; P17.5 = USB-host VBUS enable. Older names SW9/SW10, SW5/SW6 |
| base-board I2C parts (CapSense @0x08, LED matrix @0x10, OPTIGA @0x30) | SCB5 P17.0/P17.1, 3.3 V | **CS select = ON**; SCB5 is shared with display touch - one owner core or `scb5-lock` |
| pots VR1..VR4 | P15.4..P15.7, SAR ch 4..7, 0-1.8 V | knob VR1 = P15.5, VR2 = P15.4 (map {5,4,6,7}); never feed 3.3 V/5 V |
| CAN | P16.2 / P16.3 | camera data pins → camera off; **CAN/RS-485 switch = CAN** |
| Arduino D2/D3 | P13.0 / P13.5 | outputs do not latch (erratum); A0-A3, VIN, AREF not connected |

The TESAIoT SDK is **never built**; its source is only a guideline when writing these components
(cite the path in `source` / `guideline`).

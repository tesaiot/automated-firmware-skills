# TESAIoT (TESA AIoT E84) DevKit = KIT_PSE84_AI + QWA309 base board

Everything in [../kit-pse84-ai/README.md](../kit-pse84-ai/README.md) applies; this sheet adds the
QWA309 base board. Machine-readable values: `board.env` (`BOARD_EXTENDS=kit-pse84-ai`).

Sources: **[SDK]** TESAIoT SDK <https://github.com/tesaiot/tesaiot-pse84-devkit-sdk> (commit d2ebc5c,
2026-10-07): `kit-tesaiot-pse84-ai/` README + `arduino_shield_qwa309*.c`, docs J4/J5/"Peripherals
at a glance", which quote schematic **QWA309 V3_P1 (2026-03-06)** · **[HW]** tested on our kit.

> **Do not take pins from the "TESAIoT Dev Kit rev1 pinout" image** (`images/`). It disagrees with
> the SDK/schematic on most Arduino pins, the pots and the buttons - see
> `docs/hardware/tesaiot-pinout-crosscheck.md` at the repo root. Use this sheet; if a pin is missing
> here, it is not verified - ask the user or check the schematic before writing code for it.

## Verified on hardware (2026-10-03)

| Item | Value | Verified |
| --- | --- | --- |
| Debug USB / probe | KitProg3 CMSIS-DAPv2, FW 2.82.1735 (serial is per board - discover.sh) | yes |
| MCU, console, SW1, LED1-5, clocks | as kit-pse84-ai (vendor BSP 1.4.0, no overlay) | yes |
| QWA309 buttons | SW4 P17.5, SW5 P17.7 (not in BSP; run-time pull-up) | config yes (`pins?`); **read 0 permanently with the DVP camera plugged in** |
| Factory firmware | prints nothing on the KitProg3 UART; backup: `backup.sh tesaiot` (12 MB, ~35 s) | backed up |
| Power cycle after flash | not needed for console apps (display firmware needs it) | yes |

## QWA309 I/O [SDK] (not tested by us unless marked)

All SoM I/O is **1.8 V**. Arduino digital pins pass TXB0106 level shifters to 3.3 V; I2C (A4/A5)
does not (it is the 3.3 V SCB5 bus). **Never feed 3.3 V/5 V into P15.x analog inputs.**

| Function | Pins / bus | Notes |
| --- | --- | --- |
| **SW4 / SW5** | P17.5 / P17.7, active low, internal pull-up, debounce ≥ 50 ms | = DVP camera RESET/PWDN nets; P17.5 is also USB-host VBUS enable. Older names SW9/SW10 or **SW5/SW6** (the rev1 board photo shows SW5/SW6) |
| Pots VR1..VR4 | P15.4..P15.7 = SAR ch 4..7, 12-bit, 0-1.8 V | **physical VR1 = P15.5, VR2 = P15.4** (silkscreen swap): knob map `{5,4,6,7}` |
| Header I2C (A4/A5) | SCB5 P17.1 SDA / P17.0 SCL, 3.3 V | shared: display touch, QWIIC, mikroBUS ×3, OPTIGA Trust M @0x30, CapSense @0x08, LED matrix @0x10. CM55 owns it in SDK firmware; no cross-core lock |
| Header UART D0/D1 | **SCB9** P15.0 RX / P15.1 TX, 115200 | not the console |
| Header SPI D10..D13 | CS P9.0, MOSI P9.2, MISO P9.1, SCK P9.3 | owning SCB not confirmed; silkscreen "9.2" for CS is wrong |
| Arduino D2..D9 | D2 P13.0, D3 P13.5, D4 P13.6, D5 P13.4, D6 P13.3, D7 P13.7, D8 P15.3, D9 P15.2 | **D2/D3 outputs do not latch** (erratum); P13.5 has no TCPWM |
| PWM | TCPWM0 cnt5 on P13.3 (+ P13.4 complement); P15.2/P15.3 PWM-capable | |
| Arduino A0-A3, VIN, IOREF, AREF, RESET | **not connected** | |
| CapSense BTN0/BTN1 + slider | PSoC 4000T, EZI2C @0x08 on SCB5; data `[btn0][btn1][slider]` | not GPIO |
| DFR0522 16×8 RGB LED matrix | I2C @0x10 on SCB5 | no discrete GPIO LEDs on QWA309 |
| CAN (SN65HVD230) | CANFD0 ch1, P16.2 RX / P16.3 TX, 500 kbit/s | shares a slide switch with RS-485; uses camera data pins (SDK disables the DVP camera) |
| CS select switch | - | enables the base-board I2C; **must be selected** for buttons and CapSense [HW, lab 2026-10-03] |
| CAM ON/OFF switch | - | on the board photo; may release P17.5/P17.7 without unplugging the camera - **untested** |

## Hardware quirks

- DVP camera plugged in → SW4/SW5 read 0 permanently: unplug it (or try CAM OFF, untested).
- Display firmware needs a USB power cycle after flashing (debugger reset leaves the display dead).
- TESAIoT SDK requires ModusToolbox **3.6** and ships its own modified BSP 1.2.0; this skill uses
  MTB 3.9 + vendor BSP 1.4.0. BSP delta (`modus_diff.py`): pot pins P15.4-7 + SAR channels, SCB5 =
  I2C master only, m55_nvm 2.75 → 6 MB, CM55 MPU, linker scripts, `ns_start_pse84.c` - port these
  when an app needs the pots, SCB5 or a large CM55 image.

## Verification log

- 2026-10-03 `uart-btn-led` (vendor BSP 1.4.0, no overlay): build PASS 0 warnings, hex
  `1842a7c5...`, flash verified 74,204 bytes, test PASS 16/16, interactive BTN1 (AI Kit SW1) PASS 4/4.
- 2026-10-03 profile split (`tesaiot` extends `kit-pse84-ai`), app `button-led`: build PASS, flash
  verified 75 KB, auto PASS 4/4, SW1 hold → LED1 on / release → off PASS. QWA309 SW4/SW5 stuck at 0
  (DVP camera holds P17.5/P17.7 low); 60 s pin monitor saw only P7.0 toggling.
- 2026-10-03 lab: the QWA309 **CS select switch** enables the base-board I2C. Unselected, the base
  firmware prints `PSOC,ERR,<ms>,I2C,START-W,0,0x00AA2002,<n>` every 100 ms.
- 2026-10-10 desk check: QWA309 table from the TESAIoT SDK (schematic V3_P1 quotes); rev1 pinout
  image cross-checked and rejected as a pin source. Nothing new tested on hardware.
- 2026-10-10 bring-up (MTB 3.9, vendor BSP 1.4.0, apps via `new_app.sh` + VS Code workspace
  `IDE: READY`): `hello-world` build PASS 0 warnings, hex `d7c2f708...`, flash verified 72,828 bytes,
  test PASS 9/9 · `blink` hex `189c0735...`, flash verified, test PASS 6/6, LED1 blink **observed by
  a person** · `push-to-light` hex `7b73da26...`, flash verified, auto 15/16: LED commands and
  read-back PASS; `btn?` = `btn1=0 btn2=1 btn3=1` → SW4/SW5 read pressed with the DVP camera fitted
  (known issue, camera kept).
  Interactive: SW1 press → `EVT btn1=pressed led1=1` / `released`, read-back `LED led1=1 ...`,
  LED1 on **observed by a person**. The spec's `^LED led1=1$` failed only because this board
  prints 5 LEDs; fixed to `^LED led1=1( |$)` (template + app).

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
| QWA309 buttons | silkscreen SW5 P17.5 (**verified**), SW6 P17.7 (not in BSP; run-time pull-up) | config yes (`pins?`); **read 0 permanently with the DVP camera plugged in** |
| Factory firmware | prints nothing on the KitProg3 UART; backup: `backup.sh tesaiot` (12 MB, ~35 s) | backed up |
| Power cycle after flash | not needed for console apps (display firmware needs it) | yes |

## QWA309 I/O [SDK] (not tested by us unless marked)

All SoM I/O is **1.8 V**. Arduino digital pins pass TXB0106 level shifters to 3.3 V; I2C (A4/A5)
does not (it is the 3.3 V SCB5 bus). **Never feed 3.3 V/5 V into P15.x analog inputs.**

| Function | Pins / bus | Notes |
| --- | --- | --- |
| **SW4 / SW5** | P17.5 / P17.7, active low, internal pull-up, debounce ≥ 50 ms | = DVP camera RESET/PWDN nets; P17.5 is also USB-host VBUS enable. Older names SW9/SW10 or **SW5/SW6** (the rev1 board photo shows SW5/SW6) |
| Pots VR1..VR4 | P15.4..P15.7 = SAR ch 4..7, 12-bit, 0-1.8 V | **physical VR1 = P15.5, VR2 = P15.4** (silkscreen swap): knob map `{5,4,6,7}` - **verified [HW] 2026-10-10**; full turn measured only 0..~1400-1800 counts (open) |
| Header I2C (A4/A5) | SCB5 P17.1 SDA / P17.0 SCL, 3.3 V | shared: display touch, QWIIC, mikroBUS ×3, OPTIGA Trust M @0x30, CapSense @0x08, LED matrix @0x10. CM55 owns it in SDK firmware; no cross-core lock |
| Header UART D0/D1 | **SCB9** P15.0 RX / P15.1 TX, 115200 | not the console |
| Header SPI D10..D13 | CS P9.0, MOSI P9.2, MISO P9.1, SCK P9.3 | owning SCB not confirmed; silkscreen "9.2" for CS is wrong |
| Arduino D2..D9 | D2 P13.0, D3 P13.5, D4 P13.6, D5 P13.4, D6 P13.3, D7 P13.7, D8 P15.3, D9 P15.2 | **D2/D3 outputs do not latch** (erratum); P13.5 has no TCPWM |
| PWM | TCPWM0 cnt5 on P13.3 (+ P13.4 complement); P15.2/P15.3 PWM-capable | |
| Arduino A0-A3, VIN, IOREF, AREF, RESET | **not connected** | |
| CapSense BTN0/BTN1 + slider | PSoC 4000T, EZI2C @0x08 on SCB5; data `[btn0][btn1][slider]` | not GPIO |
| DFR0522 16×8 RGB LED matrix | I2C @0x10 on SCB5 | no discrete GPIO LEDs on QWA309 |
| CAN (SN65HVD230) | CANFD0 ch1, P16.2 RX / P16.3 TX, 500 kbit/s | shares a slide switch with RS-485; uses camera data pins (SDK disables the DVP camera) |
| CS select switch | - | base-board I2C side (CapSense); **must be selected** for CapSense [HW, lab 2026-10-03]; does not change any MCU GPIO [HW 2026-10-10] |
| CAM ON/OFF switch | - | **CAM ON frees SW5 (P17.5)**; CAM OFF (camera unpowered) clamps P17.5/P17.7 low; P17.7 stays low even with CAM ON [HW 2026-10-10]; does NOT free SCB5 |
| OPTIGA Trust M | I2C @0x30 on **SCB5** (P17.0/P17.1, 3.3 V) | SDK Dev Kit build sets `OPTIGA_I2C_ON_3V3_BUS` + `CYBSP_TRUSTM_I2C_SCL/SDA=P17_0/P17_1` (`proj_cm33_ns/Makefile`); the SCB0 default in its PAL is for the bare AI Kit |
| mikroBUS ×3 / QWIIC | their I2C = the same SCB5 bus | other mikroBUS signals (SPI/UART/INT/PWM per socket) **not in the SDK sources** - ask / schematic |
| Arduino connectors | J7 10-pin IOH, J8 8-pin POWER, J9 8-pin IOL, J10 6-pin ADC (schematic sheet 17) | A0-A3 reach J10 pins 1-4 and stop there (no MCU net); J8 pin 7 IOREF, VIN, RESET, AREF not connected |
| Level shifters | TXB0106 U7/U8/U9 (sheets 9-11), MCU 1.8 V ↔ header 3.3 V, all digital header pins | **no MCU internal pull-up on these pins** (it flips the shifter's direction sensing) - external pulls ≥ 50 kΩ only, **never a pull-up to 5 V** (ESD diodes clamp into +3.3V_BB; it once took down the shared I2C bus); SDA/SCL are direct 3.3 V |
| Shields | logic 3.3 V, no IOREF | shields that pick their logic level from IOREF (Adafruit Motor Shield v2 since 2024) must be strapped by hand; the SDK has an optional Motor Shield v2 (PCA9685) driver on the header I2C |

### SCB5 ownership (one bus, many users)

SCB5 (P17.0/P17.1, 3.3 V) carries display touch, QWIIC, mikroBUS ×3, OPTIGA @0x30, CapSense @0x08
and the LED matrix @0x10. In the SDK firmware **CM55 owns it** (display/GFX task; LED matrix, CapSense
via IPC to CM33) while **CM33_NS also drives it for OPTIGA - with no cross-core lock** (the SDK's
known contention). Here: one owner core per app (`claims` SCB5), or `scb5-lock` (planned) when both
cores need it. The SDK warns that a CM33 direct-I2C CapSense backend reads the wrong bus on a QWA309
board - CapSense is on SCB5, not SCB0.

### Conflicts with other TESA projects

- **E84_TouchUI** (WheelSense project, TESA team; <https://github.com/NnopponS/WheelSense_Project/tree/prototype/tesaIoT-hackathon/firmware/E84_TouchUI>)
  targets the bare AI Kit (no QWA309 code). Its production app uses the **OV7675 DVP camera**
  (P16.0-P16.7 data, P17.2-P17.5/P17.7 control, P11.1 PCLK) and the display touch on SCB5 (CM55):
  on a TESAIoT board that camera takes **SW4/SW5 (P17.5/P17.7)** and **CAN (P16.2/P16.3)**, and its
  touch shares SCB5 with the QWA309 I2C parts. Run it on TESAIoT only without the base-board buttons
  and CAN (the SDK's "DEC-A camera persona" switch does the opposite: camera off, pins to QWA309).

## Hardware quirks

- DVP camera plugged in → SW4/SW5 read 0 permanently: unplug it (or try CAM OFF, untested).
- Display firmware needs a USB power cycle after flashing (debugger reset leaves the display dead).
- The TESAIoT SDK (MTB 3.6, its own modified BSP 1.2.0) is **never used to build** here - this skill
  uses MTB 3.9 + vendor BSP 1.4.0. Its BSP delta (`modus_diff.py`) is a guideline for our own bsp
  components/overlays: pot pins P15.4-7 + SAR channels, SCB5 = I2C master only, m55_nvm 2.75 → 6 MB,
  CM55 MPU, linker scripts, `ns_start_pse84.c` - re-create the needed parts as hash-checked overlays
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
- 2026-10-10 desk check 2: SDK re-read at d2ebc5c (still the newest commit): connectors J7-J10,
  TXB0106 pull rules, IOREF/A0-A3 not connected, OPTIGA on SCB5 for the Dev Kit, SCB5 ownership;
  E84_TouchUI checked - no QWA309 use, DVP camera conflicts with SW4/SW5 + CAN. Not tested here.
- 2026-10-10 bring-up (MTB 3.9, vendor BSP 1.4.0, apps via `new_app.sh` + VS Code workspace
  `IDE: READY`): `hello-world` build PASS 0 warnings, hex `d7c2f708...`, flash verified 72,828 bytes,
  test PASS 9/9 · `blink` hex `189c0735...`, flash verified, test PASS 6/6, LED1 blink **observed by
  a person** · `push-to-light` hex `7b73da26...`, flash verified, auto 15/16: LED commands and
  read-back PASS; `btn?` = `btn1=0 btn2=1 btn3=1` → SW4/SW5 read pressed with the DVP camera fitted
  (known issue, camera kept).
  Interactive: SW1 press → `EVT btn1=pressed led1=1` / `released`, read-back `LED led1=1 ...`,
  LED1 on **observed by a person**. The spec's `^LED led1=1$` failed only because this board
  prints 5 LEDs; fixed to `^LED led1=1( |$)` (template + app).
- 2026-10-10 `qwa309-io` (pots + CapSense, bare metal on CM33): pots **verified** - SAR ch 4..7
  started by re-targeting the vendor BSP 1.4.0 AutAnalog config at run time (no overlay), `ready=1`;
  knobs turned VR1..VR4 in order gave events from P15.5, P15.4, P15.6, P15.7 (label map confirmed);
  full-turn range only 0..~1400-1800 of 4095 counts (open: board divider, or SAR sample time 31
  cycles vs pot impedance). CapSense **not working on our bench**: SCB5 SCL and SDA read 0 at idle,
  stay 0 after a 9-clock recovery and also with the MCU's internal pull-ups on -> a device holds the
  bus low. DVP camera fitted (on SCB5 as SCCB); CAM ON/OFF = OFF changed nothing. Also found: the
  vendor BSP configures SCB5 as master+slave with its own address 0x08 (= CapSense) - `i2c-scb5`
  now inits master-only (as the TESAIoT SDK does). Next: unplug the camera and re-run `bus5`.
- 2026-10-10 `qwa309-io` live view: knobs VR1..VR4 confirmed again by the user (live charts + EVT
  lines). SCB5 re-checked: still SCL=0 SDA=0 with MCU pull-ups; **CS select OFF -> still low**, so
  the clamp is not on the base-board side of the CS select switch (CapSense, LED matrix, mikroBUS);
  remaining suspects: the fitted DVP camera (CAM ON or OFF) or the display touch controller.
  Restore CS select ON for CapSense. Next: unplug the camera and re-run `bus5`.
- 2026-10-10 CapSense hunt, two TESAIoT boards ("ours", and a second board whose TESAIoT SDK
  firmware showed CapSense on the LCD). Read-only OpenOCD peek of the SDK firmware: SCB5 at
  ~100 kHz (16-bit divider /32, OVS 16+16) and camera RESET P17.5 = 1 / PWDN P17.7 = 0 driven
  (vendor BSP state); ours ran ~380 kHz with P17.5/P17.7 as pulled-up inputs. Neither difference
  is the cause: at 100 kHz and with `campins bsp` CapSense 0x08 still NACKs. The SDK firmware,
  copied onto ours (backup.sh dump -> flash.sh --hex), shows the LCD but **CapSense and SW1-SW4
  dead** and logs `I2C START-W 0x00AA2002` every 100 ms -> hardware/bench state, not our code.
  On the second board (after a backup acquire-reset and a full power cycle its CapSense no
  longer worked either) qwa309-io reads SCB5 SCL=SDA=0: **CS select OFF releases the bus after
  ~2.5 min** (then 0x10 LED matrix, 0x38 touch, 0x45 panel MCU answer), **CS select ON pulls it
  low again at once**. So a device on the CS-select side (CapSense PSoC 4000T or its supply /
  path) clamps SCB5; on ours that side no longer clamps but 0x08 never answers. Open: measure
  the CapSense supply with CS ON vs OFF / check the schematic of the CS select switch.
  With CS select OFF the rest of SCB5 (LED matrix, touch, panel MCU) works. i2c-scb5 now
  defaults to 100 kHz (= SDK).
- 2026-10-10 official release fw-mpy v2.4.8 (tesaiot-pse84-devkit-sdk, sha256 verified) flashed
  on both boards, CS select ON, CAM ON. Ours: CapSense **not detected**. Second board: detected,
  slider **drifts a lot**, CapSense buttons **not sensed**. Read-only peek of v2.4.8: SCB5 ~450 kHz
  (vendor BSP 8-bit divider /11, OVS 10+10), P17.5/P17.7 pulled-up inputs, SCL/SDA high on both -
  the same setup as our qwa309-io. => CapSense problem is in the boards' CapSense hardware
  (PSoC 4000T / pads / tuning), not in firmware. Report to the TESA hardware team.
- 2026-10-10 official C-only fw v1.11.0 (no MicroPython, sha256 verified) on both boards: CapSense
  still not working on either -> not a MicroPython issue. The SDK holds no PSoC 4000T firmware or
  programming guide (whole tree + docs searched at d2ebc5c); its SWD is not routed to the E84, and
  its API doc says button encoding "depends on firmware version". Working theory: ours = 4000T
  blank/corrupted (never ACKs 0x08), second board = 4000T firmware present but mistuned (slider
  drift, buttons not sensed). Asked the TESA hardware team for the 4000T firmware + programming
  header. Ours left on C-only v1.11.0; second board back on fw-mpy v2.4.8.
- 2026-10-10 `qwa309-buttons` (pin watch: input register of all 22 ports, 67 unused pins armed as
  plain inputs; header P9.0-3/P13/P15, mics P14, crystals P18/P19 excluded), CAM-SW ON, marked runs
  with CS select OFF then ON. Rev1 silkscreen: push buttons **SW1, SW2, SW5, SW6**; **SW3/SW4 are DIP
  switches**. **SW5 = P17.5, active low - verified** (3/3 presses, rest high with CAM ON); a press also
  resets the DVP camera (same net) -> camera D0-D7 (P16.x), VSYNC P17.2, HREF P17.3 burst after release.
  SW6: P17.7 reads 0 constantly with the camera powered -> presses invisible (unplug the camera to test).
  SW1, SW2 and DIP SW3/SW4 change **no MCU GPIO** with CS select OFF or ON -> not on a watched pin
  (maybe via I2C or on an excluded header port; ask / schematic). CAM-SW OFF clamps P17.5/P17.7 low.
- 2026-10-10 desk check 3: SDK doc site <https://tesaiot.github.io/tesaiot-pse84-devkit-sdk/sdk/mtb-mpy/>
  (J5 QWA309, J1 sensor bus, peripherals quick reference). Agrees: CapSense 4000T @0x08 sits on the
  display/touch bus (= our SCB5 P17.0/P17.1), read by CM55 in the SDK; DFR0522 @0x10 on the same bus;
  pots map {5,4,6,7}; header GPIO/PWM/UART pins; P17.5 doubles as USB-host VBUS enable. Disagrees with
  our board: the doc calls P17.5/P17.7 "SW4/SW5 = production silkscreen" (older SW9/SW10, GPIO page
  SW5/SW6) - on our rev1 board the button on P17.5 is labelled **SW5** and SW3/SW4 are DIP switches,
  so the silkscreen differs per PCB revision; it names the AI Kit user button "SW2" (our AI Kit and
  UG Rev *B: SW1, SW2 = reset). It documents only TWO base-board buttons - nothing on QWA309 SW1/SW2,
  the DIP switches or CS select. It also warns that the mtb-only (C) package's capsense_read() uses
  the direct SCB0 backend = wrong bus on a QWA309 board (the CM55 path is right) - so a C build's
  CM33-side CapSense reads nothing by design; the LCD UI reads via CM55.
- 2026-10-10 desk check 4 (SW1-SW6 in all three sources). SDK repo: only two base-board buttons,
  P17.5/P17.7 - named SW4/SW5 (mod_qwa309_buttons.c), SW9/SW10 (kit README) and **SW5/SW6**
  (mtb-only proj_cm55 page_gpio_rgb.c: "SW5 = P17.5, SW6 = P17.7", from the tested button_monitor
  reference) - the last matches our board (SW5 = P17.5 verified). "SW1" in the API docs = the AI
  Kit user button (CYBSP_USER_BTN1, P7.0); io/04 example says its silkscreen is SW2. SDK docs site:
  same two buttons, same three naming schemes. Pinout image: SW5/SW6 = D8/D9 "P8.x" (wrong - D8/D9 are
  P15.3/P15.2 per schematic V3_P1, buttons are P17.5/P17.7); SW1, SW2 and the small switches appear only
  on the photo. => No source gives a pin for QWA309 SW1/SW2 or the DIP switches SW3/SW4.
- 2026-10-10 PSoC 4000T buffer dump (second board, CS select ON, CAM ON, app qwa309-io `ezi2c 64`,
  PC-side sampling ~7 Hz with marked phases). 0x08 answers; the buffer is **not** the documented
  3-byte [btn0][btn1][slider] frame: word0 = 0x0EC5 constant, word1 = free-running scan counter
  (byte 2 = what the SDK shows as "slider" -> the drift), then 16-bit CapSense data (raw counts
  ~0x85xx, signed diffs) - looks like a CapSense tuner-style data structure. Decoded by the SDK rule
  byte0 0xC5 / byte1 0x0E -> both "1" = idle code -> buttons never pressed. Touch BTN0/BTN1/slider:
  words 2-7 and 14-22 react clearly -> the 4000T senses fine; its firmware/protocol does not match
  the SDK. SW1 and SW2 held 5 s each: **no word changes**; DIP SW3/SW4 flips: only a small
  capacitive blip on one diff word (finger proximity), no digital state -> **the 4000T does not carry
  SW1/SW2/SW3/SW4** (first 64 bytes checked; words 23-31 constant). Ours: 4000T never ACKs 0x08.
- 2026-10-10 raw5 mapping + driver test (second board). Isolated touches: BTN0 and BTN1 -> **no
  reaction**; slider left / middle / right -> segment 0 / 2 / 4 only (drops ~1500 / ~3900 / ~4400,
  neighbours rise ~250) -> the raw5 4000T firmware scans **only a 5-segment slider** (segments 0..4 =
  left..right, raw counts at buffer bytes 6..15). Words 8-12 = its baseline, 13-17 = raw-baseline,
  18-22 = positive part; its baseline is dragged down during a touch -> false touch after release.
  `qwa309-capsense` now auto-detects frame3/raw5; raw5 uses its own baseline (frozen while touched),
  thresholds 350/200, centroid 0..100. Verified: slide 0..100 in order; taps left/middle/right ->
  0 / 47-50 / 100 with clean touch/release; boot detection needs a retry (4000T NODEV right after an
  E84 reset). One run lost the right-end taps although raw counts were not checked in that run.
- 2026-10-10 live-view check of SW1-SW4 vs the 4000T protocol (second board, qwa309-io `ezreport 200`:
  FRAME b0..b2 + SDK button decode, EZW words 0..22 at 5 Hz; raw log + `dash_ctl.py mark` phases;
  `phase_diff.py`). SW1, SW2 (held 5 s), DIP SW3, SW4 (flipped + back), CapSense BTN0/BTN1: **no field
  leaves its idle range** (sdk_btn0/1 always 0); the slide (control) moves words 3-7 and 13-21.
  -> none of SW1-SW4 (nor BTN0/BTN1) is visible in the 4000T buffer. Method note: marks sent after
  the user's answer lagged the action ~2-3 s - mark the phase BEFORE asking.
- 2026-10-10 MicroPython button check (second board, fw-mpy v2.4.8, CS select ON, CAM ON; script
  polls `gpio.button(0)`, `buttons.read()`, `sensors.capsense.buttons()/slider()` every 20 ms, run
  with mpremote; 3 presses per button, one button at a time). AI Kit user button (P7.0, API "USER
  Button 1"/"SW2") 3/3; base-board SW5 = P17.5 (API `buttons` "SW4") 3/3, ~0.2-0.35 s pulses;
  SW6 = P17.7 (API "SW5") reads pressed constantly, 0/3; CapSense BTN0 0/3; base-board SW1, SW2: no
  change on any source. API `slider()` returns 0..255 wrapping every ~1.6 s at rest (doc: 0..100) =
  the raw5 scan counter. `sensors.scan()` (SCB0) lists 0x18, 0x68, 0x77 - no 0x44 although
  `sensors.init()` returns True (open: SHT40 / unknown 0x18). => the official MicroPython API sees
  exactly what our firmware sees on these inputs; no firmware gap for buttons/CapSense.
- 2026-10-11 LVGL bring-up on the second board (Waveshare 4.3" DSI on J10, CS select as left by the
  user): app `lvgl-demo` (scope dual, bare metal both cores). Panel MCU 0x45 + FT5406 0x38 on SCB5
  owned by CM55 (`i2c-scb5-cm55`), GFXSS clocks enabled on CM33_NS before CM55 starts, VG-Lite
  heap + 2 frame buffers in gfx_mem. serial_test 9/9 PASS; frame render 6-7 ms; UI correct; touch
  taps and slider drag correct (x/y mirrored).
- 2026-10-11 generated UI (ui-designer, template `lvgl-ui-app`, app with 2 screens / 19 widgets) on the
  second board: tests/ui_app.json 26/26 PASS (uitree, data bindings, simulated clicks, screen goto);
  user confirmed the screens and touch on the panel.

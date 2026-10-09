# TESAIoT Dev Kit rev1 pinout image - cross-check

Image: [images/TESAIoT Dev Kit rev1 pinout.jpg](../../images/TESAIoT%20Dev%20Kit%20rev1%20pinout.jpg)
("TESA AIoT E84 Embedded AI Board", Model 1, Version 3.1). Checked 2026-10-10 against:

- **BSP**: vendor `KIT_PSE84_AI` 1.4.0 `design.modus` (SoM pins; generated `cycfg_pins.h`)
- **SDK**: TESAIoT SDK commit d2ebc5c (2026-10-07), `kit-tesaiot-pse84-ai/` README +
  `arduino_shield_qwa309*.c` + docs J4/J5, which quote schematic **QWA309 V3_P1 (2026-03-06)**
- **HW**: our own tests (2026-10-03)

**Verdict: don't use the image as a pin source.** It contradicts itself (D0/D1 appear as both
P9.0/P9.1 and P15.0/P15.1; A4/A5 appear as both P15.6/P15.7 and "not on the header"). It also
disagrees with the SDK/schematic in 8 of the 18 rows below (7 wrong, 1 conflict), and 2 more can't
be checked yet. Several captions are garbled, so the
image looks machine-generated or hand-made rather than taken from the schematic. The board
**photo** in it is still useful: it shows the button silkscreens SW1/SW2/SW5/SW6 and the CAM ON/OFF
and OPTIGA parts.

| Signal (image) | Image says | SDK / schematic | BSP / HW | Verdict |
| --- | --- | --- | --- | --- |
| I2C SDA / SCL | P17.1 / P17.0, 3.3 V | P17.1 / P17.0 (SCB5), Arduino A4/A5 | BSP `CYBSP_I2C_SDA/SCL_3V3` | **match** |
| UART RX / TX (D0/D1) | P15.0 / P15.1 SCB9 | P15.0 RX / P15.1 TX SCB9 | - | **match** |
| SPI SS / MOSI / MISO / SCK (D10-D13) | P9.0 / P9.2 / P9.1 / P9.3 | same | BSP: P9.0-P9.3 = J5 `SERIAL_INT0..3` | **match** (owning SCB unconfirmed) |
| D7 | P13.7 | P13.7 | - | match |
| D0..D3 "Digital GPIO" | P9.0, P9.1, P9.2, P9.3 | D0 P15.0, D1 P15.1, D2 P13.0, D3 P13.5 | - | **wrong** (those are the SPI pins) |
| D4 / D5 / D6 | P13.4 / P13.5 / P13.6 | P13.6 / P13.4 / P13.3 | - | **wrong** |
| D8 / D9 | "P8.x (SW5/SW6)" | P15.3 / P15.2 | BSP: P8.x = sensor I2C, PDM mic, USB | **wrong** |
| SW5 / SW6 buttons | P8.x | SW4/SW5 on **P17.5 / P17.7** (older names SW5/SW6) | HW: P17.5/P17.7 configured, read 0 with camera | **wrong pin**; name matches the photo |
| A0..A3 | P15.2..P15.5 = VR1..VR4 | A0-A3 **not connected**; P15.2/P15.3 = D9/D8 | - | **wrong** |
| VR1..VR4 | P15.2..P15.5 | P15.4..P15.7 (ch4..7); physical VR1 = P15.5, VR2 = P15.4 | SDK BSP `CYBSP_POT_VR1..4` | **wrong** |
| A4 / A5 (header 1 table) | P15.6 / P15.7 ADC | A4/A5 = SDA/SCL (P17.1/P17.0); P15.6/P15.7 = VR3/VR4 | - | **wrong** |
| D14 / D15 (header 1) | P15.0 / P15.1 UART | these are D0/D1 | - | naming only |
| D16..D19 (header 2) | P13.0..P13.3 GPIO/PWM | P13.0 = D2, P13.3 = D6; P13.1/P13.2 not on the Arduino map (only on the J17 mezzanine) | - | **unverified** |
| "Analog inputs 1.8 V domain" | yes | SoM I/O 1.8 V; digital pins level-shifted to 3.3 V | BSP: VDDD/VDDA 1.8 V | match - **analog pins are not 3.3 V/5 V tolerant** |
| VIN "input from 5V/USB" | connected | VIN, IOREF, AREF, RESET not connected | - | **conflict** - check the board |
| 5V / 3V3 "_ADU (switched)" | switched rails | not documented | - | unverified |
| 8×16 RGB matrix (DFRobot) | on board | DFR0522 @0x10 on SCB5 | - | match |
| OPTIGA Trust M, CAM ON/OFF | on the photo | OPTIGA @0x30 on SCB5; camera nets = SW4/SW5 | HW: camera blocks SW4/SW5 | consistent |

## Open questions (resolve on the board or the schematic, then move the answer into the skill)

1. Button silkscreen on QWA309 rev1: SW5/SW6 (photo) or SW4/SW5 (SDK)? Update `BTN_LABELS`.
2. Does **CAM OFF** release P17.5/P17.7 so the buttons work with the camera fitted?
3. Is VIN connected on rev1 (image) or not (SDK schematic V3_P1)? Which revision is our board?
4. Which SCB drives the header SPI on P9.0-P9.3?
5. Header 2 pins labelled D16-D19 - what is really there on rev1?

The AI Kit's own user button: the Infineon UG Rev *B and our board both say **SW1** (P7.0), with SW2
as reset. The TESAIoT docs say "SW2", which is wrong.

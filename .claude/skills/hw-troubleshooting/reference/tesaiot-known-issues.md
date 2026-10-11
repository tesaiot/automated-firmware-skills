# TESAIoT Dev Kit - known hardware issues and how to handle them

Found on two TESAIoT boards (AI Kit + QWA309 rev1, "Version 3.1" silkscreen) on 2026-10-10. Full
evidence: the dated verification log in `.claude/skills/qwa309/boards/tesaiot/README.md`.
Symptom -> cause -> what to do. "Board A" = ours, "board B" = the second one.

## I2C bus SCB5 (P17.0 SCL / P17.1 SDA, 3.3 V)

| Symptom | Cause (evidence) | Do |
|---|---|---|
| every transfer to 0x08 times out (`MASTER_MANUAL_TIMEOUT`, rslt 0x00AA2003) | vendor BSP 1.4.0 configures SCB5 as master **+ slave with own address 0x08** = the CapSense address | `i2c-scb5` inits master-only (fixed in the component) |
| `bus5`: SCL=0 SDA=0, also with MCU pull-ups, recovery fails | a device on the CS-select side clamps the bus (board B: CS select ON -> low at once; OFF -> released after ~2.5 min); an unpowered device clamps through ESD diodes | switch CS select OFF and wait minutes; then 0x10/0x38/0x45 work. Report to the hardware team (4000T supply / CS select) |
| CAM ON/OFF = OFF does not free the bus | the camera is not the clamp on board B | - |
| speed | SDK MicroPython v2.4.8 runs ~450 kHz, an older SDK build ~100 kHz; CapSense behaved the same | `i2c-scb5` default 100 kHz (conservative) |

## CapSense PSoC 4000T @0x08

| Symptom | Cause | Do |
|---|---|---|
| board A: 0x08 never ACKs, with any firmware (ours, SDK clone, release v2.4.8, C-only v1.11.0) | 4000T blank, dead or unpowered on that board | hardware team; nothing on the E84 side |
| board B: LCD slider drifts by itself, buttons never pressed | the 4000T runs a **different firmware**: a CapSense data block ("raw5"), not the SDK 3-byte frame. Byte 2 = free-running scan counter (shown as "slider"), bytes 0-1 = constant 0x0EC5 (decoded as idle buttons) | `qwa309-capsense` auto-detects raw5 and computes the slider itself |
| raw5: BTN0/BTN1 never react | that firmware scans **only a 5-segment slider** (sensors 0..4 = left..right, raw counts at bytes 6..15) | 4000T reflash needed (firmware + SWD header: ask the hardware team) |
| raw5: false "touch" after a release in the chip's own diff words | the 4000T drags its baseline down during a touch (touch LOWERS the raw count) | ignore words 8..22; driver keeps its own baseline, frozen while touched; thresholds 350/200 counts |
| `ERR_NODEV` right after flashing / reset | 4000T still booting when the E84 starts | `capsense_init` retries 10 x 20 ms |
| C-only SDK package: CapSense reads nothing from CM33 | documented SDK packaging gap: the direct backend polls SCB0, the 4000T is on SCB5 (CM55 path is right) | - (our driver reads SCB5 directly) |

raw5 layout (16-bit LE words): w0 id 0x0EC5, w1 scan counter, w2 status, w3-w7 raw counts
(~34100 idle; touch -1500 left end .. -5000 middle; neighbours +250), w8-w12 the chip's baselines,
w13-w17 raw-baseline, w18-w22 positive part, w23-w31 constant.

## Buttons and switches

| Silkscreen (rev1) | Result | Note |
|---|---|---|
| SW5 push | **P17.5**, active low - verified | = DVP camera RESET net: a press also resets the camera (its D0-D7/VSYNC/HREF burst afterwards); P17.5 is also USB-host VBUS enable |
| SW6 push | P17.7 per SDK `page_gpio_rgb.c`; not testable while the camera is powered | = camera PWDN net, reads 0 constantly with CAM ON |
| SW1, SW2 push; SW3, SW4 DIP | **no MCU GPIO change** (pin watch, CS select ON and OFF) and **no change in the 4000T buffer** | not documented in any source - hardware-only switches? ask the hardware team |
| AI Kit SW1 | P7.0 user button (SDK docs call it "SW2"; UG + our kit: SW1, SW2 = reset) | |
| CAM ON/OFF | ON frees SW5 (P17.5); OFF = unpowered camera clamps P17.5/P17.7 low | buttons need CAM ON or the camera unplugged |

Names differ per source: the SDK calls P17.5/P17.7 SW4/SW5 (MicroPython), SW9/SW10 (README) or
SW5/SW6 (LCD GPIO page). The pinout image puts SW5/SW6 on "D8/D9 P8.x" - wrong.

## Pots

VR1..VR4 = P15.5, P15.4, P15.6, P15.7 (silkscreen swap) - verified. Full turn reaches only
~1400-1800 of 4095 counts (open: divider or SAR sample time).

## Bench procedure notes

- A board can disappear from USB (only one KitProg3 listed): ask the user to replug the
  KitProg3 USB-C with a data cable; meanwhile work on the other board.
- Backing up board B (acquire = reset) was followed by its CapSense failing; a full power
  cycle did not bring it back - the cause is unknown, but **back up and power-cycle before
  concluding**, and note every reset in the log.
- Official firmware used for comparison: tesaiot-pse84-devkit-sdk releases fw-mpy-v2.4.8 and
  fw-c-only-v1.11.0 (sha256 in their SHA256SUMS.txt); both use the same flash slots as our apps.

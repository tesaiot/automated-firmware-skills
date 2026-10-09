# PSOC Edge E84 AI Kit (KIT_PSE84_AI) - on-board I/O only

Machine-readable values: `board.env`. Parent profile of `tesaiot` (AI Kit + QWA309 base board).

Sources (checked 2026-10-10): **[BSP]** vendor BSP `KIT_PSE84_AI` 1.4.0 `design.modus` (what the
generated `cycfg_pins.h` contains) · **[UG]** Infineon kit guide 002-41506 Rev *B (2026-04-14)
<https://www.infineon.com/assets/row/public/documents/30/44/infineon-kit-pse84-ai-user-guide-usermanual-en.pdf>
· **[HW]** tested on our kit. Research notes and the TESAIoT pinout cross-check:
`docs/hardware/` at the repo root (people's notes, not a pin source).

## Verified on hardware

| Item | Value | Verified |
| --- | --- | --- |
| MCU | `PSE846GPS2DBZC4A` rev B0, EPC2, DEVELOPMENT | yes (2026-10-03) |
| BSP | vendor `KIT_PSE84_AI` 1.4.0, no overlay (clock path 0 = 24 MHz oscillator on P7.4) | yes |
| Console | SCB2, MCU RX P6.5 / TX P6.7 → KitProg3 USB-UART (USB-C **J1**), 115200 8N1 | yes |
| User button | silkscreen **SW1** = `CYBSP_USER_BTN1` (`CYBSP_SW1`) P7.0, active low. **SW2 = RESET (XRES).** The UG Rev *B agrees; the TESAIoT SDK docs call the user button "SW2" (wrong). | yes (pin monitor + button-led) |
| LEDs | LED1 green P10.7, LED2 blue P10.5 (no TCPWM route); LED3 RGB red/blue/green P20.6/P20.5/P20.4 = `CYBSP_USER_LED3/4/5` (TCPWM0 grp1 lines 265/264/263). All active high, 1.8 V | LED1 by read-back |
| Flash backup | `backup.sh <board>` reads 0x60000000 + 12 MB in ~35 s | yes |

## MCU and memory [UG][BSP] (not tested by us)

- Cores: CM55 @400 MHz + Ethos-U55 NPU (application), CM33 @200 MHz (secure + NS, NNLite).
- On chip: RRAM 512 KB, SRAM 1 MB, SoCMEM 5 MB. MCU I/O is mostly **1.8 V**.
- QSPI NOR 512 Mb S25HS512T on SMIF0 (P1.0-P1.3, CS P2.0); XIP at 0x60000000 (NS S-bus) /
  0x08000000 (NS C-bus). Octal HyperRAM 128 Mb S70KS1283 on SMIF1 (P4.0-P4.7, CS P0.1).
- Boot strap P17.6: default boots from external QSPI (R188 fitted).
- Debug: KitProg3 (PSoC 5LP) on J1, SWD P6.2/P6.3, SWO P6.0; KitProg3 FW ≥ 2.80; SW3 = mode
  button, LED4 = KitProg status. MCU USB-HS on USB-C **J2** (DRP control P7.1/7.3/7.5/7.6/8.3).

## On-board parts [UG][BSP] (not tested by us)

| Part | Bus / pins (BSP alias) | I2C addr |
| --- | --- | --- |
| BMI270 IMU | sensor I2C **SCB0 P8.0 SCL / P8.1 SDA, 1.8 V** (`CYBSP_I2C_*_1V8`); INT1 P7.7, INT2 P6.6 | 0x68 |
| DPS368 pressure | SCB0 I2C; INT P11.3 | 0x77 |
| SHT40 temp/humidity | SCB0 I2C | 0x44 |
| BMM350 magnetometer | **I3C** P3.0 SCL / P3.1 SDA; INT P6.4 | 0x15 |
| BGT60TR13C 60 GHz radar | SPI **SCB3**: MISO P21.4, MOSI P21.5, CLK P21.6, CS P21.7; IRQ P20.3; reset P20.7 | - |
| 2× PDM mic (IM73D122) | PDM_CLK P8.5, PDM_DATA P8.6 (left = falling edge, right = rising) | - |
| 2× analog mic (IM73A135) | P14.0-P14.7 (`CYBSP_AMIC_*`) | - |
| Audio codec + speaker J8 | I2S TX: MCK P21.3, SCK P21.2, SD P21.1, FSYNC P12.3 (codec probably WM8960 - unconfirmed) | 0x18 |
| CYW55513 Wi-Fi 6 / BT 5.4 (LBEE5HY2FY) | SDIO CLK P12.0, CMD P21.0, D0-D3 P12.1/12.2/12.4/12.5, WL_REG_ON P11.4; BT UART **SCB4** P10.0-P10.3, BT_REG_ON P11.0 | - |
| Display/camera I2C, 3.3 V | **SCB5 P17.0 SCL / P17.1 SDA** (`CYBSP_I2C_*_3V3`), also QWIIC J7 (not fitted) | - |
| DVP camera (J14, Rev ** only) | D0-D7 P16.0-P16.7, VSYNC P17.2, HREF P17.3, XCLK P17.4, RESET P17.5, PWDN P17.7, PCLK P11.1. From Rev *A the kit ships a USB camera on J2 instead | - |
| MIPI-DSI display | J10 (Raspberry Pi compatible) | - |

Expansion on the SoM: J5 (6-pin, not fitted) = P9.0-P9.3 "SERIAL_INT0..3" (VDD 1.8 V);
J17 DF40C 20-pin mezzanine (not fitted on the AI Kit) = P15.0-P15.7 + P13.0-P13.7, 1.8 V - this
is where a base board (QWA309) picks up its I/O.

## Verification log

- 2026-10-03 `uart-btn-led` and `button-led` (via the `tesaiot` profile, same AI Kit): build PASS,
  flash verified, SW1 hold → `EVT btn1=pressed name=SW1 pin=P7.0 led1=1`, release → `led1=0`.
- 2026-10-10 desk check: pin aliases above read from the BSP 1.4.0 `design.modus`; part list,
  addresses and connectors from UG Rev *B. Not tested on hardware.

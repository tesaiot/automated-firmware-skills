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

## MCU, memory, clocks, power [UG][BSP] (not tested by us)

- Cores: CM55 @400 MHz + Ethos-U55 NPU (application), CM33 @200 MHz (secure + NS, NNLite).
- On chip: RRAM 512 KB, SRAM 1 MB, SoCMEM 5 MB.
- QSPI NOR 512 Mb **S25HS512TFABHI013** (U4) on SMIF0 (D0-D3 P1.0-P1.3, CS P2.0); XIP at
  0x60000000 (NS S-bus) / 0x08000000 (NS C-bus). BSP image regions end at offset 0x00880000;
  hybrid erase map 4 KB / 128 KB at the bottom, 256 KB above; page 256 B.
- Octal PSRAM 128 Mb **S70KS1283GABHI020** (U3) on SMIF1 (D0-D7 P4.0-P4.7, CS P0.1, RWDS,
  differential clock); **P0.0 = memory reset** (flash + PSRAM). Not in the BSP memory map.
- Clocks: 24 MHz oscillator Y3 on P7.4 (EXT_CLK, clock path 0), ECO crystal Y1 17.2032 MHz on
  P19.0/P19.1, WCO 32.768 kHz Y2 on P18.0/P18.1. Radar: own 80 MHz oscillator U24. Wi-Fi: 32.768 kHz
  LPO Y4.
- I/O voltage per port: P0-P15, P18-P21 = **1.8 V**; **P16, P17 = 3.3 V** (VDDIO6).
- Power in: J1 (KitProg3 USB-C, 5 V), J2 (MCU USB-C, 5 V), J3 3-5 V battery/external (not fitted,
  + D15). Rails: 5 V boost U29 (USB host + regulators), 3.3 V buck U13 (radio VBAT, 3.3 V I/O, audio
  DAC, DSI display), 1.8 V buck U34 (core + I/O), 3.3 V LDO U35 + 1.8 V LDO U33 (radar, analog).
  Power LED LED5 (amber).
- Boot strap P17.6: R188 fitted = external QSPI (default); R187 = internal RRAM.
- Debug: KitProg3 = PSoC 5LP CY8C5868LTI-LP039 (U2) on J1: SWD P6.2 SWDIO / P6.3 SWCLK, SWO P6.0,
  TDI P6.1, XRES; USB-UART on P6.5/P6.7 and **USB-I2C bridge on P8.0/P8.1 (= sensor bus SCB0)**;
  SW3 = mode button, status LED **D12** (amber); KitProg3 FW >= 2.80. J4 10-pin SWD/JTAG (not
  fitted) for MiniProg4.
- MCU USB-HS on USB-C **J2**, device or host (DRP controller U21, host current limit 500 mA):
  ID P7.1, DIR P7.3, OUT1/OUT2 P7.5/P7.6, over-current CF P8.3; status LEDs D11 = host (DFP),
  D14 = device (UFP) (UG figure; its text says D12).

## On-board parts [UG][BSP] (not tested by us unless the log says so)

| Part (ref) | Bus / pins (BSP alias) | Address | bsp component |
| --- | --- | --- | --- |
| BMI270 6-axis IMU (U18) | sensor I2C **SCB0 P8.0 SCL / P8.1 SDA, 1.8 V** (`CYBSP_I2C_CONTROLLER`, `CYBSP_I2C_*_1V8`); INT1 P7.7, INT2 P6.6 | 0x68 (0x69: R85 in, R86 out) | `imu-bmi270` |
| DPS368 pressure + temp (U6) | SCB0; INT `PSEN_INT` P11.3 | 0x77 (0x76: R84) | `pressure-dps368` |
| SHT40 humidity + temp (U22) | SCB0; no INT | 0x44 | `humidity-sht4x` |
| Audio DAC (U28), speaker J8 (not fitted) | control SCB0; I2S TX on TDM0 (`CYBSP_TDM_CONTROLLER_0`): MCK P21.3, SCK P21.2, SD P21.1, FSYNC P12.3. Class-D mono 2.5 W + headphone driver; analog in TP24/TP25. UG gives no part number - **TLV320DAC3100** per the kit's Infineon I2S example | 0x18 | `audio-tlv320dac3100` |
| BMM350 magnetometer (U20) | **I3C** `CYBSP_I3C_CONTROLLER` (I3C_CORE) P3.0 SCL / P3.1 SDA; MAG_INT P6.4 (I2C mode: rework R220/R223) | 0x15 | `mag-bmm350` |
| BGT60TR13C 60 GHz radar (U5) | SPI **SCB3** (`CYBSP_SPI_CONTROLLER`, `CYBSP_RSPI_*`): MISO P21.4, MOSI P21.5, CLK P21.6, CS P21.7; IRQ P20.3; reset `RXRES_L` P20.7 | - | `radar-bgt60` |
| 2x IM73D122 PDM mic (U8 left, U7 right, ~27 mm) | PDM0: PDM_CLK P8.5, PDM_DATA P8.6; left = falling edge = BSP channel 2, right = rising = channel 3 | - | `mic-pdm` |
| 2x IM73A135 analog mic (U10 left, U9 right, ~32 mm) | CTB: AMIC1 INP/INN/OUT/REF P14.0/P14.1/P14.2/P14.6, AMIC2 P14.4/P14.5/P14.3/P14.7 (no BSP aliases) | - | `mic-analog` |
| CYW55513 Wi-Fi 6 / BT 5.4 (Murata 2FY LBEE5HY2FY) | SDHC0 SDIO CLK P12.0, CMD P21.0, D0-D3 P12.1/12.2/12.4/12.5, WL_REG_ON P11.4, WL_HOST_WAKE P11.6; BT UART **SCB4** TX P10.0 / RX P10.1 / RTS P10.2 / CTS P10.3, BT_REG_ON P11.0, BT_HOST_WAKE P10.4, BT_DEV_WAKE P10.6 (UG table 2 also lists P11.2 "WL BT DEV WAKE") | - | `radio-cyw55513` |
| 3.3 V I2C | **SCB5 P17.0 SCL / P17.1 SDA** (`CYBSP_I2C_CAM_CONTROLLER` = `CYBSP_I2C_DISPLAY_CONTROLLER`, `CYBSP_I2C_*_3V3`) to J10, J16, QWIIC J7 (not fitted), J14 | - | `i2c-scb5` |
| MIPI-DSI display | J10 (R-Pi 15-pin, 2 lanes, 1.5 Gbps/lane, 3.3 V); J16 (not fitted) = 5 V + I2C for touch (J16.1 5V, .2 SCL, .3 GND, .4 SDA). Supported: Waveshare 2.8/4/4.3/5/7" DSI (4.3" and 7" need J16) | panel MCU 0x45, FT5406 0x38 | `display-dsi` |
| Camera | Rev *A+: 0.3 MP USB camera (HBVCAM OV7675 USB, UVC) on **J2** (MCU = USB host). Before Rev *A: OV7675 DVP on header **J14** (3.3 V, depopulated from Rev *A): D0-D7 P16.0-P16.7, VSYNC P17.2, HREF P17.3, XCLK P17.4, RESET P17.5, PWDN P17.7, PCLK P11.1 (input-only, level-shifted) | - | `camera-usb-uvc` |
| QSPI flash / PSRAM | see above | - | `qspi-flash`, `psram` |
| User LEDs / button | see "Verified on hardware" (RGB PWM: TCPWM0, `CYBSP_PWM_LED_CTRL`) | - | `ai-kit-board`, `led-rgb-pwm` |

Expansion on the SoM: J5 (6-pin 2.54 mm, not fitted) = P9.0-P9.3 "SERIAL_INT0..3" (1.8 V; with
R76/R77/R79/R80 an alternate serial/programming interface; config pins P20.1/P20.2);
J17 Hirose DF40C-20DP-0.4V (not fitted on the AI Kit) = analog-capable P15.0-P15.7 + P13.0-P13.7,
1.8 V - this is where a base board (QWA309) picks up its I/O.

## Verification log

- 2026-10-03 `uart-btn-led` and `button-led` (via the `tesaiot` profile, same AI Kit): build PASS,
  flash verified, SW1 hold → `EVT btn1=pressed name=SW1 pin=P7.0 led1=1`, release → `led1=0`.
- 2026-10-10 desk check: pin aliases above read from the BSP 1.4.0 `design.modus`; part list,
  addresses and connectors from UG Rev *B. Not tested on hardware.
- 2026-10-10 desk check 2: full UG Rev *B pass (power, clocks, I/O voltages, USB-C, rework
  options, address straps). Audio DAC = TLV320DAC3100 inferred from Infineon
  `mtb-example-psoc-edge-i2s` (codec lib on `CYBSP_I2C_CONTROLLER` = SCB0) - confirm on hardware.
  USB camera model from `mtb-example-psoc-edge-ml-deepcraft-deploy-vision`. KitProg3 status LED is
  D12 (was "LED4" here). The BSP has no `CYBSP_AMIC_*` aliases (was claimed here).
- 2026-10-10 `sensors` (scope cm33, via the `tesaiot` profile, same AI Kit): 17/17 PASS. SCB0 scan
  0x18 0x44 0x68 0x77 (**audio DAC answers at 0x18**). SHT40 39.9 °C / 47.0 %RH, DPS368 101107 Pa /
  42.0 °C, BMI270 |a| = 992 mg at rest, BMM350 on I3C reads + self-test OK. BMM350 shows a constant
  ~2.5 mT offset that turns with the board (90° flat turn changed x/y/z by 12-28 µT only) - hard-iron
  calibration needed; open: does it come from the AI Kit or from the QWA309 underneath (test a bare kit).
- 2026-10-10 `sensors2` (same kit): BMM350 with the address fallback and the Bosch fix applied by
  `components.py patch` (no bash PREBUILD) - 17/17 PASS, `sensor-debug diag` PASS, self-tests OK.

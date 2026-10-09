/*******************************************************************************
* File Name : qwa309.h
*
* Layer 1 - board extras of the TESAIoT QWA309 base board (not in the AI-Kit
* BSP). Include it ONLY from board/ code that drives a base-board part (pots,
* SW4/SW5, header UART/SPI/GPIO, CAN, LED matrix, CapSense ...).
*
* Board-scope gate (compile time): the bare PSOC Edge E84 AI Kit
* (BOARD_KIT_PSE84_AI) cannot reach these parts - building such code for it
* stops here. Re-create the app with board id "tesaiot". Same rule as
* scripts/board_gate.py (prompt hook, new_app.sh, build.sh).
* Pins: TESAIoT SDK, schematic QWA309 V3_P1 - see boards/tesaiot/README.md.
*******************************************************************************/
#ifndef QWA309_H
#define QWA309_H

#include "board.h"

#if !defined(BOARD_HAS_QWA309)
#error "BOARD GATE: QWA309 base-board part used, but this app is not built for the TESAIoT Dev Kit (board id tesaiot). The bare PSOC Edge E84 AI Kit cannot reach base-board parts - re-create the app with: new_app.sh tesaiot <app>"
#endif

/* Buttons SW4 / SW5: active low, MCU pull-up, = DVP camera RESET / PWDN nets
 * (read "pressed" while the camera is fitted). P17.5 is also USB-host VBUS en. */
#define QWA309_SW4_PORT_NUM     (17U)
#define QWA309_SW4_PIN          (5U)
#define QWA309_SW5_PORT_NUM     (17U)
#define QWA309_SW5_PIN          (7U)

/* Pots VR1..VR4 on P15.4..P15.7 = SAR ch 4..7 (0-1.8 V). Silkscreen swap:
 * knob VR1 = P15.5 (ch5), VR2 = P15.4 (ch4) -> knob-to-channel map below. */
#define QWA309_POT_PORT_NUM     (15U)
#define QWA309_POT_CHANNELS     { 5U, 4U, 6U, 7U }   /* VR1, VR2, VR3, VR4 */

/* Header UART (Arduino D0/D1): SCB9, P15.0 RX / P15.1 TX - not the console */
#define QWA309_UART_SCB_NUM     (9U)
#define QWA309_UART_PORT_NUM    (15U)
#define QWA309_UART_RX_PIN      (0U)
#define QWA309_UART_TX_PIN      (1U)

/* Header I2C (Arduino A4/A5) = SCB5 P17.1 SDA / P17.0 SCL, 3.3 V, shared with
 * display touch; 7-bit addresses of the base-board parts on it */
#define QWA309_I2C_ADDR_CAPSENSE    (0x08U)
#define QWA309_I2C_ADDR_LED_MATRIX  (0x10U)
#define QWA309_I2C_ADDR_OPTIGA      (0x30U)

#endif /* QWA309_H */

/*******************************************************************************
* File Name : board.h
*
* Layer 1 - BSP adaptation (board layer).
* The ONLY layer that may use CYBSP_* aliases, PDL (Cy_*) pin/SCB calls or
* board-specific #ifdefs. Everything above (func/, main.c) uses this API only,
* so porting to another PSOC Edge E84 board touches this folder (plus the BSP).
*
* Board selection: the build passes -DBOARD_<ID> (set by new_app.sh from the
* board profile): BOARD_KIT_PSE84_AI, BOARD_TESAIOT.
* Names are the SILKSCREEN names users see, not the BSP macro names.
*******************************************************************************/
#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>
#include <stdint.h>

#if defined(BOARD_TESAIOT)
    /* TESAIoT DevKit = PSOC Edge E84 AI Kit + QWA309 base board.
     * AI-Kit I/O from the BSP, plus the base-board buttons configured at run time. */
    #define BOARD_NAME          "tesaiot"
    #define BOARD_HAS_QWA309    (1)
    /* btn1 = AI Kit SW1 (P7.0); btn2/btn3 = QWA309 rev1 silkscreen SW5 (P17.5, verified
     * 2026-10-10) / SW6 (P17.7, from the photo - masked while the camera holds PWDN low).
     * The SDK calls them SW4/SW5. The QWA309 also has push buttons SW1/SW2 - not on MCU GPIO. */
    #define BOARD_BTN_NAMES     "SW1", "SW5", "SW6"
    #define BOARD_LED_NAMES     "LED1", "LED2", "RGB_RED", "RGB_BLUE", "RGB_GREEN"

#elif defined(BOARD_KIT_PSE84_AI)
    /* Infineon PSOC Edge E84 AI Kit, on-board I/O only.
     * CYBSP_USER_BTN1 = CYBSP_SW1 = user button P7.0, silkscreen "SW1" on the
     * kit in our lab (SW2 there is RESET; TESAIoT SDK docs say "SW2" - trust the board). */
    #define BOARD_NAME          "kit-pse84-ai"
    #define BOARD_BTN_NAMES     "SW1"
    #define BOARD_LED_NAMES     "LED1", "LED2", "RGB_RED", "RGB_BLUE", "RGB_GREEN"

#else
    #define BOARD_NAME          "generic-pse84"
#endif

/* BOARD_BSP_BUTTONS caps how many CYBSP_USER_BTNx aliases are real buttons
 * (a BSP may declare I/O the board does not populate). Default: all of them. */
#ifndef BOARD_BSP_BUTTONS
    #define BOARD_BSP_BUTTONS   (2U)
#endif

/* Upper bounds for static tables in func/ */
#define BOARD_MAX_LEDS          (5U)
#define BOARD_MAX_BUTTONS       (4U)

/* System */
void     board_init(void);               /* cybsp_init, IRQs, console UART      */
void     board_start_cm55(void);         /* boot the CM55 core (boot chain)     */
void     board_delay_ms(uint32_t ms);        /* FreeRTOS: vTaskDelay once running */
uint32_t board_millis(void);             /* ms tick: SysTick (bare metal) or kernel tick (FreeRTOS, 0 until the scheduler runs) */
void     board_fatal(void);              /* stop execution on unrecoverable err */

/* User LEDs, index 0..board_led_count()-1 (LED1 = index 0) */
uint32_t    board_led_count(void);
void        board_led_write(uint32_t idx, bool on);
bool        board_led_read(uint32_t idx);         /* read back the pin output   */
const char *board_led_name(uint32_t idx);         /* silkscreen / user name     */

/* User buttons, index 0..board_button_count()-1 (BTN1 = index 0) */
uint32_t    board_button_count(void);
bool        board_button_is_pressed(uint32_t idx);  /* raw, not debounced      */
const char *board_button_name(uint32_t idx);        /* silkscreen name, "SW2"  */
const char *board_button_pin(uint32_t idx);         /* e.g. "P17.5"            */
/* Diagnostics: "drive=<dm> hsiom=<n> in=<0|1> out=<0|1>" of the button pin */
const char *board_button_diag(uint32_t idx);

/* Console UART (debug UART routed to the probe's USB-UART). printf() also works. */
bool     board_console_getc(char *c);    /* non-blocking; true if a byte read   */
void     board_console_flush(void);      /* wait until TX FIFO is empty         */

#endif /* BOARD_H */

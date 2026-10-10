/*******************************************************************************
* File Name : radio.h                         component: radio-cyw55513 (planned)
*
* Layer 1 - BSP. Murata Type 2FY (LBEE5HY2FY) module = Infineon AIROC CYW55513:
*   Wi-Fi 6 (802.11ax, 2.4/5 GHz, SDIO 3.0) + Bluetooth 5.4 (BR/EDR/LE, HCI UART)
*   of the PSOC Edge E84 AI Kit. VBAT 3.3 V, I/O 1.8 V, 32.768 kHz LPO (Y4),
*   on-board chip antenna.
*   Wi-Fi: SDHC0 SDIO CLK P12.0, CMD P21.0, D0-D3 P12.1/P12.2/P12.4/P12.5,
*          WL_REG_ON P11.4, WL_HOST_WAKE P11.6.
*   BT:    SCB4 UART TX P10.0, RX P10.1, RTS P10.2, CTS P10.3,
*          BT_REG_ON P11.0, BT_HOST_WAKE P10.4, BT_DEV_WAKE P10.6.
* Backend: the BSP does the SDIO/WHD bring-up when CYBSP_WIFI_CAPABLE is
*   defined; Wi-Fi via wifi-core-freertos-lwip-mbedtls + wifi-connection-manager
*   (cy_wcm_*), BT via btstack-integration (CYBSP_BT_UART). Both need an RTOS:
*   exec_models = freertos only.
* This bsp component only powers the radio and starts the stacks; protocols
* (MQTT, HTTP, GATT) are func components (e.g. wifi-mqtt).
*******************************************************************************/
#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

/* Wi-Fi (station). Blocking calls: run from an RTOS task, not an ISR. */
bsp_status_t radio_wifi_init(void);                       /* cy_wcm_init (STA) */
bsp_status_t radio_wifi_connect(const char *ssid, const char *pass,
                                uint32_t timeout_ms);
bsp_status_t radio_wifi_disconnect(void);
bool         radio_wifi_is_connected(void);
bsp_status_t radio_wifi_ip(char *buf, uint32_t len);      /* dotted IPv4       */
int32_t      radio_wifi_rssi(void);                       /* dBm, 0 if down    */
/* Scan: cb called per AP found (ssid, rssi, channel); returns count. */
uint32_t     radio_wifi_scan(void (*cb)(const char *ssid, int32_t rssi, uint8_t ch),
                             uint32_t timeout_ms);

/* Bluetooth LE: power up the controller and the stack; mgmt_cb is the
 * btstack management callback (void* = wiced_bt_management_cback_t). */
bsp_status_t radio_bt_init(void *mgmt_cb, const void *stack_cfg);

/* Power control (WL_REG_ON / BT_REG_ON) for low-power apps. */
bsp_status_t radio_power(bool wifi_on, bool bt_on);

uint32_t     radio_last_rslt(void);

#endif /* RADIO_H */

/*******************************************************************************
* File Name : camera_uvc.h                    component: camera-usb-uvc (planned)
*
* Layer 1 - BSP, CM55. 0.3 MP USB camera module (HBVCAM OV7675 USB, UVC)
*   shipped with the PSOC Edge E84 AI Kit from Rev *A, on the MCU USB-C port **J2** (USB HS, DRP controller U21
*   switches to host/DFP and powers the camera; LED D11 = host mode).
*   USB-C DRP signals: ID P7.1, DIR P7.3, OUT1/OUT2 P7.5/P7.6, fault CF P8.3;
*   host current limit 500 mA. (Kits before Rev *A: OV7675 DVP camera on J14
*   instead - not covered here.)
* Backend: Infineon emusb-host UVC on CM55, as proj_cm55/source/usb_camera_task.c
*   of mtb-example-psoc-edge-ml-deepcraft-deploy-vision (camera VID/PID set
*   there; v2.2.0 enabled the 24 MHz EXT_CLK to cut USB transaction errors).
*   Needs an RTOS: exec_models = freertos only. Owns USBHS -> conflicts with
*   usb-cdc. DVP alternative (pre-Rev *A): Infineon camera-dvp-ov7675.
*   Licence: emUSB-Host is a binary-EULA library (excluded so far in
*   docs/components.md) - get the owner's decision before implementing.
* Frames land in a caller buffer (PSRAM recommended: psram.h).
*******************************************************************************/
#ifndef CAMERA_UVC_H
#define CAMERA_UVC_H

#include <stdbool.h>
#include <stdint.h>
#include "bsp_status.h"

typedef enum
{
    CAM_FMT_YUYV = 0,
    CAM_FMT_MJPEG
} cam_format_t;

typedef struct
{
    uint16_t     width;       /* 640 / 320 */
    uint16_t     height;      /* 480 / 240 */
    cam_format_t format;
    uint8_t      fps;
} cam_config_t;

#define CAM_CONFIG_DEFAULT  { 320U, 240U, CAM_FMT_YUYV, 15U }

bsp_status_t cam_init(const cam_config_t *cfg);    /* start USB host stack    */
bool         cam_is_connected(void);               /* UVC device enumerated   */
bsp_status_t cam_start(void);
bsp_status_t cam_stop(void);

/* Wait up to timeout_ms for the next frame; copies at most max bytes. */
bsp_status_t cam_get_frame(uint8_t *buf, uint32_t max, uint32_t *len,
                           uint32_t timeout_ms);

uint32_t     cam_last_rslt(void);

#endif /* CAMERA_UVC_H */

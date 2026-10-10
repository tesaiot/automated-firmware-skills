# Core scope and execution model per core

Two independent choices, both made by the user (`new_app.sh --scope S --model cm33=..,cm55=..`,
or the `web-wizard` project form):

| Choice | Components | Meaning |
| --- | --- | --- |
| core scope | `scope-cm33` (verified), `scope-dual` (verified), `scope-cm55` (planned) | which cores run application code; the others are parked (`idle`) or a boot shim (`boot`) |
| model, CM33_NS | `baremetal-cm33` (verified), `freertos-cm33` (verified) | super-loop + SysTick, or FreeRTOS tasks + kernel tick |
| model, CM55 | `baremetal-cm55` (built), `freertos-cm55` (verified) | super-loop + busy-wait, or FreeRTOS tasks + CM55 kernel tick |
| IPC (scope dual) | `ipc-mailbox` (verified), `ipc-none` (built), `ipc-pipes` / `mtb-ipc-queue` (planned) | how the cores exchange data, or explicitly not at all |

Matrix (verified 2026-10-10 on TESAIoT unless marked):

| Scope | CM33_NS | CM55 | Template | Status |
| --- | --- | --- | --- | --- |
| cm33 | baremetal | idle | hello-world, blink, push-to-light, button-led, sensors | verified |
| cm33 | freertos | idle | - (no template yet) | gates pass, untested |
| dual | baremetal | baremetal | dual-core-ipc | built, not run |
| dual | freertos | freertos | rtos-dual (11/11) | verified |
| dual | baremetal | freertos / freertos + baremetal | - | gates pass, needs a template |
| cm55 | boot | baremetal / freertos | - | planned (`scope-cm55`) |

The gate (`components.py`): every application core has exactly one model component; a parked core
has none and runs no app/driver code; func/bsp components list the models they were tested under
(`exec_models`); the template lists the variants its `main.c` files implement (`scopes.txt`,
`components.py variant`).

## Issues when bare metal and RTOS are selectable per core

| # | Issue | How it is handled now | Open |
| --- | --- | --- | --- |
| 1 | **App code shape differs**: a super-loop `main()` vs tasks + `vTaskStartScheduler()` - the same `main.c` cannot serve both | each template declares its variants in `scopes.txt`; `new_app.sh` refuses others | a portable app contract (`app_init()` + `app_poll()` run by either model, optional `app_tasks()`) so one template covers several variants |
| 2 | **Tick ownership**: SysTick is the bare-metal 1 ms tick and the FreeRTOS kernel tick | the tick belongs to the CM33 model: `ai-kit-board` keeps `SysTick_Handler` only without `COMPONENT_FREERTOS`; under FreeRTOS `board_millis()` = kernel tick, `board_delay_ms()` = `vTaskDelay` once running. Each core has its own SysTick, so CM55 never collides | - |
| 3 | **Time before the scheduler**: `board_millis()` stays 0 until `vTaskStartScheduler()`; code that waits on it before (e.g. `ipc_client_call`) would hang | start-up order documented (init → mailbox → start CM55 → tasks → scheduler); IPC calls only in tasks | an assert in `ipc_client_call` when the scheduler is not running |
| 4 | **Interrupt priorities**: ISRs that call `...FromISR` must have NVIC priority ≥ 2 on CM33 (0x40) and ≥ 1 on CM55 (0x20) - 3 priority bits | written in both `FreeRTOSConfig.h` and the manifests; drivers' IRQs today don't call kernel APIs | a check in components.py: components declare `irq_priority` + whether they call the kernel |
| 5 | **Thread safety of shared drivers** (I2C bus, console, printf) | `RTOS_AWARE` makes retarget-io/HAL locking RTOS-aware; `i2c0_set_lock()` hook for a mutex; console/led/button: poll from ONE task | an exec-provided mutex helper so apps don't wire `i2c0_set_lock` by hand |
| 6 | **Blocking calls**: bsp drivers use bounded busy-waits (`Cy_SysLib_Delay`, I2C polling) - fine in bare metal, but they burn the CPU under an RTOS and starve lower priorities | acceptable for short waits (≤ 10 ms); tasks sized so console/blink keep running | interrupt/DMA-driven variants for long transfers (radar, audio) |
| 7 | **IPC between different models**: polling mailbox works both ways, but an RTOS side should wait on a semaphore, a bare-metal side polls | CM55 FreeRTOS worker polls once per tick (1 ms); CM33 `ipc_client_call` busy-waits ≤ 100 ms | IPC interrupt + semaphore (`mtb-ipc-queue`, needs FreeRTOS on both cores) |
| 8 | **Low power**: tickless idle needs the LPTimer (MCWDT) per core and Deep Sleep coordination across cores; bare metal never sleeps | tickless off on both cores (`configUSE_TICKLESS_IDLE 0`) - the Infineon CEs turn it on because the BSP idle mode is Deep Sleep | a `lowpower` option: LPTimer setup + `vApplicationSleep` as in the CEs |
| 9 | **Memory**: two heaps/stacks per core; CM33_NS SRAM is small; heap_3 uses newlib malloc | 50 KB kernel heap setting (heap_3 = newlib heap from the linker); stack high-water mark shown by `tasks` | per-app heap sizing; CM55 buffers in SoCMEM/PSRAM |
| 10 | **Testing and debugging**: the matrix grows (scope × model per core); RTOS-aware debugging differs | each variant needs its own template test; FreeRTOS thread awareness via RTOS_AWARE in VS Code | CI-style build of every template variant |

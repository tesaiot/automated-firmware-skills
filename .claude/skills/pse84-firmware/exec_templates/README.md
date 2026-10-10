# exec_templates - execution model and inter-core IPC

**Core scope** (`"scope"`): exactly one per app - `scope-cm33` (CM33_NS only, CM55 parked),
`scope-dual` (both cores, needs an IPC template: default `ipc-mailbox` via `default_with`, or
`ipc-none`), `scope-cm55` (CM33_NS boot shim, planned). Keys: `scope_title`, `scope_jobs`,
`app_cores`, `parked` ({core: idle|boot}), `needs_kind` (`ipc`), `scope_allows`.
**Execution model per application core**: `baremetal-cm33` / `freertos-cm33`, `baremetal-cm55` /
`freertos-cm55` (`exec_model`). FreeRTOS components ship `FreeRTOSConfig.h` + `exec/rtos_hooks.c`.
Issues and matrix: `../reference/exec-models.md`.
Exactly one execution model per core (`exec_model`: `baremetal`, `idle`, `boot`, `freertos`, ...), plus the
CM33_NS <-> CM55 contract (`kind: ipc`: shared-memory mailbox, mtb-ipc pipes). Owns the main
loop / scheduler start, tick, task layout and shared-memory placement. Never touches CM33_S or the
BSP's own mtb-ipc/SRF channel 0. Schema and workflow: `../reference/components.md`.

# exec_templates - execution model and inter-core IPC

**Core scope** (`"scope"`): exactly one per app - `baremetal-cm33` (CM33_NS only, CM55 parked),
`baremetal-dual` (both cores, needs an IPC template, default `ipc-mailbox` via `default_with`),
`baremetal-cm55` (CM33_NS boot shim, planned). Keys: `scope_title`, `scope_jobs` (per-core job),
`needs_kind` (`ipc`), `scope_allows` (CM33_NS components allowed in a boot-shim scope).
Exactly one execution model per core (`exec_model`: `baremetal`, `idle`, `boot`, `freertos`, ...), plus the
CM33_NS <-> CM55 contract (`kind: ipc`: shared-memory mailbox, mtb-ipc pipes). Owns the main
loop / scheduler start, tick, task layout and shared-memory placement. Never touches CM33_S or the
BSP's own mtb-ipc/SRF channel 0. Schema and workflow: `../reference/components.md`.

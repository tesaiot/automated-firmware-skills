# Software components - bsp / func / exec templates

An app = **components** (reusable, per layer) + an **app template** (application layer only).
The agent composes; `scripts/components.py` resolves and gates; `new_app.sh` applies.

```
<pack>/bsp_templates/<id>/   hardware: pins, peripherals, drivers behind board.h-style APIs
                             (packs psoc-e84-ai-kit = SoM alone, qwa309 = base board)
func_templates/<id>/         services + middleware + custom logic (LVGL, sensors, protocols...)
exec_templates/<id>/         execution model (baremetal, FreeRTOS...) and inter-core IPC
templates/<app>/             main.c (+ proj_cm55/main.c), tests/*.json, components.txt, requires.txt
```
func/exec live in this engine skill (MCU-generic); packs may also carry their own func/exec
components. Component ids are unique across the engine and all packs.

Extra manifest keys for hardware: `"claims": ["SCB5", "P17.5"]` (pins/peripherals; one claimer per
resource), `"arbitrates": ["SCB5"]` (allows several claimers, e.g. `scb5-lock`),
`"hw_settings": ["QWA309 CS select switch = ON"]` (printed as `ACTION: JUMPER`, stored in
`APP_HW_SETTINGS`, shown by `flash.sh` as `CHECK HW:`). A board's `BOARD_COMPONENTS` are always
added (e.g. `tesaiot` → `qwa309-board`).

Layer rules (same as reference/layering.md): only **bsp** code may use `CYBSP_*`, `Cy_*` pin/SCB
calls and `#if defined(BOARD_*)`; **func** uses bsp/func APIs only; **exec** owns the loop/RTOS
and the CM33↔CM55 contract; the app template wires them. CM33_S (secure) is never touched.
bsp APIs return `bsp_status_t` (`board/bsp_status.h`, shipped by `ai-kit-board` / `cm55-board`),
use integer units and bounded waits; a `planned` component may ship its header only (the API
contract), the `.c` comes with the implementation. Conventions: `<pack>/bsp_templates/README.md`.

## component.json

```json
{
  "id": "led-matrix",                 // = folder name, unique across layers
  "layer": "bsp",                     // bsp | func | exec  (= parent folder)
  "kind": "ipc",                      // optional sub-kind (exec: ipc)
  "status": "planned",                // planned -> built (0-warning build) -> verified (on hardware)
  "cores": ["cm33_ns"],               // cm33_ns | cm55 (cm33_s: never)
  "summary": "one line for list/help",
  "api": "board/led_matrix.h",        // public header(s) in files/ (bsp: returns bsp_status_t)
  "provides": ["led-matrix"],         // capability names (informational)
  "requires": {
    "components": ["ai-kit-board", "i2c-scb5"],   // resolved transitively, applied first
    "parts": ["led-matrix"]                       // part ids from the packs' parts.json -> board-scope gate
  },
  "exec_model": {"cm33_ns": "freertos"},          // exec components only: model per core
  "scope": "cm33",                                // exec: core scope cm33 | dual | cm55 (one per app)
  "scope_jobs": {"cm33_ns": "...", "cm55": "..."},// exec: what each core does in this scope
  "needs_kind": "ipc", "default_with": ["ipc-mailbox"],   // exec: dual needs an IPC template
  "options": {"flash_area": {"question": "...", "recommended": "fs-10mb",
              "choices": {"fs-10mb": {"label": "...", "defines": {"proj_cm33_ns": ["X=1"]}}},
              "custom": {"align": "0x40000", "min_offset": "...", "max_end": "...", "defines": {}}}},
  "exec_models": ["baremetal", "freertos"],       // func/bsp: models it can run under
  "conflicts": ["other-id"],
  "patches": [{"lib": "BMM350_SensorAPI", "file": "bmm350.c", "find_re": "...", "replace": "...",
               "applied_re": "...", "why": "...", "source": "..."}],   // vendor-library fixes (build.sh)
  "mtb_deps": {"proj_cm33_ns": {"freertos": "https://github.com/Infineon/freertos#release-vX#$$ASSET_REPO$$/freertos/release-vX"}},
  "makefile": {"proj_cm33_ns": {"COMPONENTS": ["FREERTOS"], "DEFINES": [], "INCLUDES": ["../shared"]}},
  "source": "where the code came from (repo + path + commit)",
  "license": "licence of the copied code",
  "verified": "date, board, app, result",
  "notes": "integration notes"
}
```

`files/` mirrors the app: `files/proj_cm33_ns/func/x.c`, `files/proj_cm55/...`, `files/shared/...`.
`apply` copies it, writes `proj_*/deps/<name>.mtb` from `mtb_deps` (getlibs fetches them on the
first build: `build.sh <app> --getlibs`) and appends `KEY+=value` lines to `proj_*/Makefile`.

## Gates (`components.py check <board> <ids...>`, also run by new_app.sh before Project Creator)

| Check | BLOCKED when |
| --- | --- |
| resolve | unknown id, missing dependency |
| status | a `planned` component (override only with `--allow-planned` while developing it) |
| exec model | two different models on one core (e.g. `baremetal` + `freertos` on CM33_NS), none selected, or a func/bsp component that can't run under the selected model |
| conflicts | two components that exclude each other |
| board parts | a component needs a part the board lacks (QWA309 parts on `kit-pse84-ai`) - same data as board_gate.py |
| cores | `files/` or `makefile` target a project the app does not have |
| core scope | none / two scopes; CM55 code under `cm33`; `dual` without an IPC template; CM33_NS code under `cm55` → `ACTION: CHOOSE core scope` |
| options | a component option not chosen or invalid (`--opt key=value`) → `ACTION: CHOOSE <key>` |

## Workflow for the agent (custom templates → components → integration)

1. Requirement → capabilities (e.g. "show pot values on the LED matrix every 100 ms").
2. `components.py list --all` → map capabilities to components; read their README/notes.
3. Missing capability → create a component (`planned`), sourcing code from (in order) an existing
   component, an Infineon code example/library, the TESAIoT SDK source **as a guideline only**
   (rewrite into our layers; never build with the SDK or add it as a dependency; cite it in `source`,
   keep its licence terms), new code.
   Pins only from `<pack>/boards/<id>/README.md`; parts listed in `requires.parts`, pins and
   peripherals in `claims`, switch settings in `hw_settings`. Hardware code goes into the pack of
   that hardware (SoM pack if it works on the SoM alone, base-board pack otherwise).
4. `components.py check <board> <ids...>` until `COMPONENTS: PASS`.
5. App template: `main.c` wires the components under the chosen exec model + `tests/<app>.json`.
6. `new_app.sh <board> <app> "" <template>` (or `--components`), build (0 warnings → component
   `built`), flash + test (→ `verified`, with the evidence in the manifest).

Catalog of available and candidate components: `docs/components.md` (repo root).

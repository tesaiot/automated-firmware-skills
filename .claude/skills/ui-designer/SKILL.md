---
name: ui-designer
description: AI-assisted LVGL UI design for PSOC Edge E84 / TESAIoT apps with the Waveshare 4.3" DSI display - a local browser editor (800x480 canvas approximating LVGL's default theme, widget tree, properties, drag/resize) whose free-text requests ("add a slider bound to level, orange") are handled by this Claude Code session as JSON patches, and whose Submit turns ui.json into static LVGL 9.5 C code (ui_gen.py), builds, flashes and checks it on the board (uitree, data set/get, simulated clicks). Use when the user wants to design, change or generate a touch UI / screen / GUI for an E84 app (ออกแบบหน้าจอ, UI, LVGL), or to turn a UI JSON into code. Not for apps without the display.
---

# ui-designer - design in the browser, Claude edits, code is generated

```
browser page  --(edit / command / Submit)-->  ui_server.py  <--(wait / patch / say)--  this session
                                                  |  ui.json (source of truth, undo history)
Submit:  ui_gen.py -> proj_cm55/ui/ui_gen.c ... -> build.sh -> flash.sh -> serial_test (tests/ui_app.json)
         -> uitree on the board -> dashed device boxes on the canvas
```

Decisions (user, 2026-10-11): canvas = HTML approximation (no LVGL-in-WebAssembly); the AI is this
Claude Code session (no API key); output = **static C code** the developer owns and extends - no
JSON loader on the board.

## Status

Verified 2026-10-11 on tesaiot (Waveshare 4.3" DSI), app from `lvgl-ui-app`: page command ->
session patch -> canvas; Submit -> ui_gen -> build (0 warnings) -> flash -> `tests/ui_app.json`
26/26 PASS; the user confirmed the screens and touch on the board; device boxes vs canvas within
3 px for 17 of 19 widgets.

## Files

| Path | Content |
| --- | --- |
| `reference/ui-spec.md` | the JSON format, actions, data binding, `#id` patches - **read before editing a spec** |
| `scripts/ui_spec.py` | validate + lint (`check`), outline with pointers (`tree`), JSON Patch |
| `scripts/ui_gen.py` | `ui.json` -> C (`ui_gen.c/.h`, `ui_conf.h` fonts, `shared/ui_ids.h`, `ui_actions.c` stubs, `tests/ui_app.json`) |
| `scripts/ui_server.py` | editor server 127.0.0.1:8767, page + queue + validation + undo, chat log `designer-chat.jsonl` |
| `scripts/ui_ctl.py` | session side: `wait`, `show`, `patch`, `replace`, `say`, `device`, `quit` |
| `web/designer.html` | the editor page |
| `examples/lvgl-demo.json` | 2 screens, bindings, goto, user events (starter of template `lvgl-ui-app`) |

App template: `pse84-firmware/templates/lvgl-ui-app` (scope dual, bare metal both cores; CM55 runs
the generated UI, CM33 console `ui`, `uitree`, `uidata`, `uiget`, `uiset`, `uiscreen`, `uiclick`
over the mailbox). Its `post_create.sh` runs `ui_gen.py` on `ui/ui.json` when `new_app.sh` creates
the app.

## Workflow

1. **App**: an existing app made from `lvgl-ui-app`, or create one (scope/model is the user's
   choice; the template offers `dual cm33_ns=baremetal cm55=baremetal` only):
   `new_app.sh tesaiot <app> "" lvgl-ui-app --scope dual --model cm33=baremetal,cm55=baremetal`.
   Needs board `tesaiot` (or an AI Kit with the panel): template `requires.txt` = `display`.
   Other apps: `ui_gen.py` needs `proj_cm55` + `proj_cm33_ns`, the `lvgl-ui` + `display-dsi`
   components and the CM55 main loop calling `ui_gen_create()` / `ui_gen_ipc()` - copy from the template.
   **A new app starts with the demo UI** (`examples/lvgl-demo.json`, 2 screens, 18 widgets, stubs
   `ui_on_led_sw_value_changed` + `ui_on_reset_btn_clicked` in `ui_actions.c`). Build on it, or
   start empty: `ui_server.py <app>/ui/ui.json --reset <name>` (old spec kept as `ui.json.bak`) and,
   on a fresh app only, delete `proj_cm55/ui/ui_actions.c` before the next `ui_gen.py` (it is
   recreated; otherwise the demo stubs show up as `GEN: ORPHAN`).
2. **Open the editor** (only when the user asks for it / agrees) - background:
   `python .claude/skills/ui-designer/scripts/ui_server.py <app>/ui/ui.json` (`--new <name>` creates
   the file only if it does not exist; `--no-open` + the built-in browser
   `preview_start url=http://127.0.0.1:8767/`). Layout warnings (outside the display, overlap,
   touch target < `MIN_TOUCH_PX`) need the page open: it measures the boxes and posts them; without
   a browser only the structural checks of `ui_spec.py check` run - size touch targets yourself.
3. **Listen** - background, always re-arm after handling an item:
   `python .claude/skills/ui-designer/scripts/ui_ctl.py wait --timeout 3000`
   It exits with `UI: COMMAND id=N rev=R selected=<id>` (`selected` = the widget id selected on the
   page, or `-`) + `UI: TEXT ...`, or `UI: SUBMIT id=N [note=...]`, or `UI: IDLE` (re-arm if the
   user is still designing). Headless (tests, scripts, no page): POST the request yourself -
   `POST /api/ask {"text": "...", "selected": null}`, `POST /api/submit {"note": ""}` - then run
   `wait` in the foreground; it returns at once with the queued item.
4. **COMMAND** (you are the AI designer):
   - `ui_ctl.py show` (outline with pointers, editor boxes, warnings); the user's `selected` widget is
     usually what "this" means.
   - Write the smallest RFC 6902 patch with `#id` paths **with the Write tool** to the scratchpad
     (never a heredoc: backslashes get mangled), then
     `ui_ctl.py patch <file> --cmd N --say "<one or two sentences: what changed and any choice you made>"`.
     `UI: REJECTED` lists the errors - fix and resend; nothing was changed.
   - Design rules: prefer flex containers over pixel positions; touch targets >= 40 px
     (`MIN_TOUCH_PX`; 4.3" panel at 130 DPI); keep everything inside 800x480; reuse the theme
     colours; new value displays bind to `data` instead of needing code; only fonts that matter
     (each size costs flash); ASCII text.
   - Unclear request -> `ui_ctl.py say --cmd N --keep-open "question"`; the user answers with a new
     command (close the old one with `say --cmd N "..."`).
   - Behaviour that needs code (talk to CM33, sensors, timers) -> event action `"user"` (a stub in
     `ui_actions.c`) and/or a `data` item with a `source` note; say so in the reply. Do not invent
     IPC commands in the spec.
5. **SUBMIT** (= the user's go for generate + build + flash + test of this app on the attached board;
   any other firmware/board still needs its own OK):
   `ui_ctl.py say --cmd N --keep-open "building..."`, then
   `ui_gen.py <app>/ui/ui.json <app>` -> `build.sh <app>` -> `flash.sh <app> --yes` ->
   `serial_test.py <port> <app>/tests/ui_app.json <log> --sync --board <id>` ->
   `ui_ctl.py device <log>` (dashed device boxes + `DIFF` lines) -> `ui_ctl.py say --cmd N "<result>"`.
   A Submit note may restrict it ("only build"); with no board attached (or none assigned to you)
   stop after `BUILD:` and say so in the reply. Relay `ACTION: JUMPER` lines as usual. Report
   `GEN: STUB added` / `GEN: ORPHAN` lines - orphans are the developer's to delete.
   `tests/ui_app.json` checks what the spec defines (widgets, bindings, built-in actions, that a
   `clicked` handler fires); a new `"user"` event on another event type adds no step - test the
   developer's handler in their own steps.
   Offer the interactive steps (`--only-interactive`: "screen matches", "tap ...") - they need the user.
6. **Stop**: `ui_ctl.py quit` when the user is done (frees port 8767).

## Generated code contract

- `proj_cm55/ui/ui_gen.c/.h`, `ui_conf.h`, `shared/ui_ids.h`, `tests/ui_app.json`: rewritten by every
  `ui_gen.py` run - never edit them; change `ui.json`.
- `proj_cm55/ui/ui_actions.c`: the developer's file. Created once; later runs only append stubs for
  new `"user"` events and report orphans. Handlers run in the LVGL context (CM55 main loop): use
  `ui.<id>`, `ui_data_set/get`, any `lv_*` call; never call LVGL from an ISR or from CM33.
- CM33 reaches the UI only through the mailbox (`IPC_CMD_UI`, `UI_ARG(op, idx, val)`, see
  `shared/ui_ids.h`): e.g. a pot value -> `UI_OP_SET`; a user action CM33 must act on -> CM33 polls a
  `data` item (`UI_OP_GET`), because the mailbox is CM33 -> CM55 request/reply only.
- `lv_conf.h` (component `lvgl-ui`) includes `ui_conf.h` when present, so only the fonts the spec
  uses are compiled.

## Open items

- Layout lint without a browser: a Python approximation of the canvas layout so headless runs also
  get the outside / overlap / touch-target warnings (clean-room 2026-10-11, finding 2).
- Images (C arrays, memory placement, VG-Lite tiled-image patch).
- Screens created on demand instead of all at start, for large UIs.

## Accuracy of the canvas

The canvas imitates LVGL's default theme at 130 DPI (large display): paddings, radii, default
sizes, flex, Montserrat (web font; boxes are measured after it loads). Verified 2026-10-11 against
the device (`uitree` on the tesaiot display board): heights exact, positions within 1-2 px, text
widths within ~3 % (LVGL's bitmap Montserrat is a little wider). Labels showing data differ while
the device's value differs. After every Submit the device boxes are drawn dashed, with the offset
for any widget > 3 px off - trust the device.

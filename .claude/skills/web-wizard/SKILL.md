---
name: web-wizard
description: Browser forms instead of one-by-one console questions - opens a local page, waits for the user's submit and returns all answers at once. "project" mode is a new-app wizard for pse84-firmware (board, template, core scope + bare metal/FreeRTOS per core, IPC, on-board hardware drivers, services, component options, then generate + VS Code / build / build + flash + test) with the real component gates checked before submit; "ask" mode renders any set of questions (AskUserQuestion-like JSON). Use when several choices are needed at once, when creating or bringing up a new PSOC Edge E84 / TESAIoT app, or when the user prefers a web form ("open the wizard", "ask me in the browser", "เปิดหน้าเว็บให้เลือก").
---

# web-wizard - answer many questions on one page

Same pattern as the `sensor-debug` live view: a local stdlib server, one page, the agent waits for
the result. The page only collects answers - all actions still run through the gated
`pse84-firmware` scripts afterwards.

## When to use it (vs. console questions)

When **two or more** decisions are pending (typical: a new app = board, template, scope, models,
hardware, options, next step), offer both, once per session, and remember the choice:
"Answer in the browser (one form) or here (console questions)?" Use AskUserQuestion for that
question. A single quick decision stays in the console. Never open the browser without the
user's consent.

## Project wizard (new app)

```bash
python .claude/skills/web-wizard/scripts/wizard.py project --board tesaiot [--app my-app]
```
Run it in the background (`run_in_background: true`); it opens the default browser and exits
when the user submits, cancels, or after `--timeout` (900 s). The last stdout line is
`WIZARD: ANSWERS {json}` | `WIZARD: CANCELLED` | `WIZARD: TIMEOUT`.

The form is built from live data, so it never offers what the engine can't do:
- boards from the packs; templates with their description and needed parts
- **cores and execution model**: only the variants listed in the template's `scopes.txt` (scope +
  bare metal / FreeRTOS per core), with the per-core jobs of the scope
- IPC choice for scope dual (`ipc-mailbox`, `ipc-none` = independent cores; planned ones disabled)
- on-board hardware = bsp components of the chosen board's packs; services = func components
  (planned ones visible but disabled - later: display/LVGL, Wi-Fi/MQTT, web...)
- component options (e.g. `flash_area`) with the recommended choice preselected
- next step: `generate_vscode` | `build` | `build_flash_test`
- **Check** = `components.py check` with the selection (same gates as `new_app.sh`); Submit runs
  it again and refuses a selection the gates block.

Answers JSON: `board, app, template, scope, models{core: model}, ipc, components[], options{},
action, ids[], command, check{rc, lines}, unused[{id, used_by_templates[]}]`.

`unused` = parts ticked on top of the template: their drivers are compiled in, but the template's
`main.c` does not call them (the page labels them and offers templates that use them). When it is
not empty, tell the user before acting, and after generation offer to add the calls to the app code
(Stage 3: init in `main()`, a console command / periodic print, extend `tests/<app>.json`) - never
claim the app uses a part it only links.

Then:

| `action` | What you do |
| --- | --- |
| `generate_vscode` | run `command` (new_app.sh ...); `open_ide.sh <app>`; tell the user the workspace is open |
| `build` | run `command`, then `build.sh <app>`; report `BUILD:` |
| `build_flash_test` | run `command`, `build.sh`, `discover.sh <board>` if no bench file, `flash.sh <app> --yes`, `serial_test.py auto <app>/tests/<spec>.json ...`. Picking this option in the form is the user's approval for **that one** flash of that app to that board; any further flash needs a new OK |

Relay every `ACTION: JUMPER` / `CHECK HW:` line before flashing, exactly as without the wizard.
If the user cancels or it times out, fall back to console questions.

## Generic questions

```bash
python .claude/skills/web-wizard/scripts/wizard.py ask questions.json [--out answers.json]
```
`questions.json` (see `examples/questions.json`):
`{"title": "...", "questions": [{"id", "header", "question", "multi": false, "other": true,
"options": [{"label", "description"}]}]}`. Every question needs an answer (a choice or "Other").
Answer: `{"mode": "ask", "answers": {"<id>": {"question", "selected": label | [labels], "other"}}}`.

## Files

| Path | Content |
| --- | --- |
| `scripts/wizard.py` | server: `project` / `ask`, `/data.json`, `/check`, `/submit`, `/cancel` (127.0.0.1:8766) |
| `web/project.html` | new-app wizard page |
| `web/ask.html` | generic question page |
| `examples/questions.json` | example spec for `ask` |

# Clean-room run 2026-10-11: ui-designer

- **Prompt:** `../prompts/ui-designer.md`. Thermostat app `ui-clean-test` on board tesaiot, run
  headless (requests POSTed to the page API), stopped after the build.
- **Agent:** general-purpose, no conversation context. 34 tool calls, about 5 min.
- **Verified by the session:** no file under `.claude/`, `CLAUDE.md` or `docs/` changed during the
  agent's run (11:22-11:26 by modification time; every later change is the triage below); the app is
  in `apps/` (git-ignored).

## Stages

| Stage | Result | Evidence |
| --- | --- | --- |
| App creation (`new_app.sh ... lvgl-ui-app --scope dual --model ...`) | PASS | GATE, VARIANT and COMPONENTS PASS, `Created`, 48 s |
| Editor server (`--no-open`) | PASS | started; `ui_ctl.py quit` freed the port |
| Request -> patch loop | PASS | 3 requests, `UI: APPLIED` rev 2..4, `selected` honoured |
| Submit -> ui_gen | PASS | 2 submits (one with the note "only build"), stub added, orphans reported |
| Build | PASS | 3x `make exit=0 warnings=0 errors=0`, `BUILD: PASS` |
| Developer-code survival | PASS | the filled handler stayed byte-identical after a regenerate that appended a new stub |

Result: 2 screens, 18 widgets, 3 data items, 2 user events, fonts 14/24/48. `tests/ui_app.json`
has 24 steps (generated, not run: no board assigned).

## Findings and triage

| # | Finding | Class | Fix | Status |
| --- | --- | --- | --- | --- |
| 1 | A new app starts with the demo UI and demo stubs; no documented way to start empty (`--new` acts only when the file is missing) | script-fix + doc-fix | `ui_server.py --reset NAME` (old spec -> `.bak`); SKILL.md step 1 says to delete `ui_actions.c` on a fresh app | done |
| 2 | Headless runs get no layout lint: the boxes come from the page; touch-target and overlap checks silently absent | doc-fix + feature | SKILL.md step 2 says which checks need the page; Python layout approximation listed under "Open items" | doc done, feature open |
| 3 | `children` listed as a common key, but only container and button take it | doc-fix | ui-spec.md: children rule + the transparent-container pattern for a label on an arc | done |
| 4 | Negative `inc.by` not documented | doc-fix | ui-spec.md: "negative = decrement", clamped | done |
| 5 | Headless pattern (POST, then foreground `wait`) and the meaning of `selected` not documented | doc-fix | SKILL.md step 3 | done |
| 6 | What to do on Submit with no board attached | doc-fix | SKILL.md step 5: stop after `BUILD:` and say so | done |
| 7 | A new non-click `"user"` event adds no test step | accepted | SKILL.md step 5 says what the generated tests cover | done |
| 8 | pse84-firmware Stage 2 usage line omits `--model` | doc-fix | added to the usage line | done |
| 9 | `WARN: ... Dropbox folder` from `new_app.sh` | noise | pse84-firmware Stage 2 now says it is advice only | done |

The fixes changed wording plus one new option (`--reset`, tested on a copy: the spec was reset and
`.bak` written). The workflow itself is unchanged, so no second clean-room run is needed.

Afterwards the session flashed `ui-clean-test` to the tesaiot display board: `tests/ui_app.json`
24/24 PASS, and the user confirmed the screens, touch and the developer handler (the Heating label
changes colour) on the panel.

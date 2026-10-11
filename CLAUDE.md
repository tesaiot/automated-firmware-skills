# TESAIoT repo - notes for Claude

- Firmware work on KIT_PSE84_AI / TESAIoT boards: use the project skill `pse84-firmware`
  (`.claude/skills/pse84-firmware/SKILL.md`). Board ids: `kit-pse84-ai`, `tesaiot`.
- **Pins come only from the skill's board sheets** (`.claude/skills/<pack>/boards/<id>/README.md`: packs `psoc-e84-ai-kit`, `qwa309`)
  or the BSP. Never from `images/*pinout*.jpg` (known wrong) or `docs/hardware/` open questions.
  If a pin is not in the board sheet, ask instead of guessing.
- **Board-scope gate:** the bare PSOC Edge E84 AI Kit (`kit-pse84-ai`) has no QWA309 base-board parts
  (pots, SW4/SW5, LED matrix, CapSense, headers, CAN...). A prompt hook, `board_gate.py`, `build.sh` and
  `board/qwa309.h` block such requests/apps; relay the block and offer board `tesaiot` - never bypass it.
- **Bottom-up, components, no TESAIoT SDK builds:** compose apps from `bsp_templates/`,
  `exec_templates/`, `func_templates/` (`components.py`). Never build with the TESAIoT SDK or add it as a
  dependency; its source is only a guideline for writing component templates (cite it in `source`).
- When a hardware fact gets verified, add a dated line to that board's verification log and remove
  the matching open question from `docs/hardware/tesaiot-pinout-crosscheck.md`.
- `apps/` and `keepout/` are never committed (see `.gitignore`); neither are probe serials, COM ports
  or flash dumps.
- **Core scope and the execution model per core are the user's choice** (`new_app.sh --scope
  cm33|dual|cm55 --model cm33=baremetal|freertos,cm55=...`); so are component options such as the
  `qspi-flash` `flash_area`. On `ACTION: CHOOSE` ask - never pick silently. With several open
  choices offer the `web-wizard` browser form or console questions.
- Sensor problems on hardware: use the `sensor-debug` skill (PC drives the console; no flashing);
  physical stimulus tests need the user - ask for one action at a time. Open the browser live
  view (`sensor_dash.py`) only when the user asks; stop it afterwards (it holds the COM port).
- Live view = raw data first: its bridge writes every board line to `<app>/logs/liveview-*.log`;
  read that (`sensor-debug/scripts/dash_ctl.py log|cmd|mark`) instead of stopping the view or asking
  what the page shows. Mark each user action before asking for it.
- Board faults (dead device, clamped bus, unknown button pin, "is it firmware or hardware"): use the
  `hw-troubleshooting` skill; known TESAIoT findings are in its `reference/tesaiot-known-issues.md`.
- Touch UI on the DSI display: `ui-designer` skill (browser editor; this session answers its
  requests via `ui_ctl.py wait` -> patch; Submit = ui_gen.py -> build -> flash -> tests/ui_app.json).
  Generated `ui_gen.*` are never hand-edited; developer code goes in `ui_actions.c`.
- Before committing a new/changed skill or workflow: `cleanroom-test` skill (fresh subagent follows
  only the docs; triage its report into doc/script fixes; record in `cleanroom-test/reports/`).
- Backlog: `docs/jobs-to-be-done.md`.

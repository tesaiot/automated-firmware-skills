You are a new firmware developer on this repo (the repo root, Windows, Git Bash + Python available, ModusToolbox installed). You know nothing about it beyond what its docs say. Your goal is to test whether the documented workflow for designing a touch UI works for someone without prior context.

Task: create a new TESAIoT app (board id `tesaiot`) named `ui-clean-test` that shows a touch UI: a "Thermostat" screen with a big temperature readout (data item `temp_c`, -20..60), a target setpoint shown on an arc the user can drag (data `setpoint`, 10..30, init 22), "+" and "-" buttons changing the setpoint by 1, a "Heating" switch (data `heat_on`) whose change calls developer code, and a second "Info" screen reachable by a button and back.

Rules:
- Use ONLY what the docs tell you: start from CLAUDE.md in the repo root, then the skill files it points to (`.claude/skills/ui-designer/SKILL.md` and what that references, `.claude/skills/pse84-firmware/SKILL.md` as needed). Do not search the conversation history or other sessions.
- Core scope / execution model: use `--scope dual --model cm33=baremetal,cm55=baremetal` (the user's pre-made choice for this test).
- Do the design through the documented editor loop, headless: start `ui_server.py` with `--no-open` in the background. Play the user by POSTing a request to the server's page API exactly as the page would (`POST http://127.0.0.1:8767/api/ask` with JSON `{"text": "...", "selected": null}`), describing the thermostat in plain words. Then act as the AI designer as SKILL.md describes: `ui_ctl.py wait`, `ui_ctl.py show`, write a JSON Patch with the Write tool, `ui_ctl.py patch ... --cmd N --say ...`. You may send more than one request. Then play the user pressing Submit (`POST /api/submit` with `{"note": ""}`) and handle the SUBMIT item as documented, BUT STOP after the build: do NOT flash and do NOT run serial tests (no board is assigned to you in this test). Reply to the submit item with `ui_ctl.py say` and then stop the server (`ui_ctl.py quit`).
- Fill the generated `ui_on_..._value_changed` stub for the heat switch with a trivial body (e.g. change a label colour) so you exercise the developer-owned file, regenerate once more, and confirm your edit survived and the build still passes.
- Do NOT modify anything under `.claude/`, `CLAUDE.md` or `docs/`. Work only in `apps/ui-clean-test` and your own scratch files. If the docs are wrong or a script fails, work around it if you can, and record it.
- Do not commit anything.

Report back (concise, as your final message):
1. PASS/FAIL per stage: app creation, editor server, request -> patch loop, submit -> ui_gen, build, developer-code survival.
2. Final widget/data/screen counts and the BUILD line (warnings/errors).
3. Every point where the docs were missing, unclear, misleading or wrong, or a command failed - with the exact command/output and what you had to guess. This is the most important part.
4. Time spent per stage roughly.

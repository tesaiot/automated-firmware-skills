# TESAIoT repo - notes for Claude

- Firmware work on KIT_PSE84_AI / TESAIoT boards: use the project skill `pse84-firmware`
  (`.claude/skills/pse84-firmware/SKILL.md`). Board ids: `kit-pse84-ai`, `tesaiot`.
- **Pins come only from the skill's board sheets** (`.claude/skills/pse84-firmware/boards/<id>/README.md`)
  or the BSP. Never from `images/*pinout*.jpg` (known wrong) or `docs/hardware/` open questions.
  If a pin is not in the board sheet, ask instead of guessing.
- **Board-scope gate:** the bare PSOC Edge E84 AI Kit (`kit-pse84-ai`) has no QWA309 base-board parts
  (pots, SW4/SW5, LED matrix, CapSense, headers, CAN...). A prompt hook, `board_gate.py`, `build.sh` and
  `board/qwa309.h` block such requests/apps; relay the block and offer board `tesaiot` - never bypass it.
- When a hardware fact gets verified, add a dated line to that board's verification log and remove
  the matching open question from `docs/hardware/tesaiot-pinout-crosscheck.md`.
- `apps/` and `keepout/` are never committed (see `.gitignore`); neither are probe serials, COM ports
  or flash dumps.
- Backlog: `docs/jobs-to-be-done.md`.

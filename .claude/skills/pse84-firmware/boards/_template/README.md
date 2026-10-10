# Adding a new PSOC Edge E84 board

1. **Collect sources:** schematic, vendor BSP or reference `design.modus`, board manual.
   Record: MCU MPN + EPC category, debug connector + probe type, console UART pins and where
   they go (probe USB-UART or header), LED/button pins and active levels, crystal (ECO)
   frequency and any external clock, external flash/RAM parts.
2. **Pick the base BSP** with the same MPN/EPC (`project-creator-cli --list-boards`):
   `KIT_PSE84_AI`, `KIT_PSE84_EVAL_EPC2`, ...
3. **Copy this folder** to `boards/<id>/`, fill `board.env`. Unknown values → `# UNVERIFIED`.
4. **Probe first:** `discover.sh <id>` (read-only) lists probes and prints the detected device -
   copy the MPN into `EXPECTED_DEVICE` (and confirm EPC2/EPC4) before it can PASS. The probe
   serial and COM port go to the bench file, not into `boards/`.
5. **Base app without overlay:** `new_app.sh <id> probe-<id> apps none` → note the base
   `design.modus` sha256 and BSP version; compare clocks/pins with the board sources
   (`scripts/modus_diff.py`). If the clock tree differs, port it (reference/project.md).
   Do **not** flash until clocks + console + boot memory are confirmed for this board.
6. **Overlay:** save the edited `design.modus` here, record both hashes in `board.env`.
7. **Smoke test:** `new_app.sh <id> hello-<id> "" hello-world` → build → (ask) flash → test.
8. **Document** in `boards/<id>/README.md` (copy the kit-pse84-ai structure): connections, user
   I/O table, BSP delta, quirks, and a verification-status line with the date.
9. Add `BOARD_<ID>` → `BOARD_NAME` in `bsp_templates/ai-kit-board/files/proj_cm33_ns/board/board.h`, and a row in
   SKILL.md's board table.

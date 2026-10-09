# Jobs to be done

Goal: a developer with a TESAIoT Dev Kit (or a bare KIT_PSE84_AI) asks Claude Code for a feature
and gets it built, flashed and tested on the board, with every pin coming from a verified source.

## Done

- 2026-10-10 bring-up on TESAIoT: hello-world, blink, push-to-light built, flashed and tested
  (see the tesaiot board sheet's verification log). VS Code workspaces generated for each app.

## Now - make the base board usable (hardware on the bench)

1. **Resolve the pinout open questions** in
   [hardware/tesaiot-pinout-crosscheck.md](hardware/tesaiot-pinout-crosscheck.md): button silkscreen
   (SW4/SW5 vs SW5/SW6), whether CAM OFF frees P17.5/P17.7, VIN, and the SPI SCB. Get the QWA309
   V3_P1 schematic PDF if possible.
2. **Test SW4/SW5** with the camera off or unplugged and the CS select switch on → `button-led` PASS on
   all three buttons; fix `BTN_LABELS`.
3. **Publish a corrected pinout** (rev1 image v2) generated from `boards/tesaiot/README.md`, so the
   image and the skill can't drift apart.

## Next - QWA309 drivers (M2 board support)

4. Pots: SAR ADC on P15.4-P15.7 with the `{5,4,6,7}` knob map → port the BSP delta (pot pins + SAR
   channels) as a hash-checked `design.modus` overlay (`BSP_OVERLAY_MODUS` in `board.env`).
5. Header I2C (SCB5) bring-up: scan → expect 0x08 CapSense, 0x10 LED matrix, 0x30 OPTIGA. Decide
   which core owns SCB5 (the SDK gives it to CM55, with no lock).
6. On-board sensors on SCB0 (BMI270 0x68, DPS368 0x77, SHT40 0x44): `WHO_AM_I`-style tests.
7. Templates: `pot-read`, `led-matrix`, `sensor-hub` (UART output + `tests/*.json`).

## Later

8. **Sync with upstream** [automated-firmware-skills](https://github.com/vsupacha/automated-firmware-skills)
   `modus-pdl-edgitalk`: it is newer (open_ide.sh, blink/push-to-light, exit code 10 / `ACTION:`
   contract, milestones.env) but needs the repo-level `lib/` and `boards/`. Decide: vendor those
   in or keep this self-contained copy. Send the board-sheet updates from here back upstream.
   (Upstream also has a stray file `skills/modus-pdl-edgitalk/scripts,\nOUT=` to delete.)
9. Decide the ModusToolbox version: the TESAIoT SDK needs MTB 3.6 + its own BSP 1.2.0, while this
   skill uses MTB 3.9 + vendor BSP 1.4.0. Pick one per app type (SDK demos vs skill apps).
10. CM55 + NPU path: start from `mtb-example-psoc-edge-ml-*` examples via `examples.sh`; display
    apps need the power-cycle step after flashing.
11. Upstream `push-to-light` test spec has the same `^LED led1=1$` bug (fails on boards with
    more than one LED) - send the fix back.

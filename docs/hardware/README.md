# Hardware notes

Research and evidence for people. Verified facts the agent uses are in the skill's board sheets:
[kit-pse84-ai](../../.claude/skills/psoc-e84-ai-kit/boards/kit-pse84-ai/README.md) ·
[tesaiot](../../.claude/skills/qwa309/boards/tesaiot/README.md).

| File | Content |
| --- | --- |
| [tesaiot-pinout-crosscheck.md](tesaiot-pinout-crosscheck.md) | rev1 pinout image vs SDK/schematic vs BSP, row by row, plus open questions |

## Sources

| Key | Source | Use for |
| --- | --- | --- |
| UG | Infineon KIT_PSE84_AI kit guide 002-41506 Rev *B - <https://www.infineon.com/assets/row/public/documents/30/44/infineon-kit-pse84-ai-user-guide-usermanual-en.pdf> | AI Kit parts, connectors, addresses |
| RN / QSG | Release notes / quick start (same folder: `...-release-notes-usermanual-en.pdf`, `...-qsg-usermanual-en.pdf`) | kit revisions (Rev *A: USB camera replaces DVP), MTB ≥ 3.5 |
| BSP | <https://github.com/Infineon/TARGET_KIT_PSE84_AI> (`config/design.modus`) | **ground truth for SoM pins** |
| Zephyr | <https://docs.zephyrproject.org/latest/boards/infineon/kit_pse84_ai/doc/index.html> | clocks, boot source |
| SDK | <https://github.com/tesaiot/tesaiot-pse84-devkit-sdk> - `bento_libs/claw/kit-tesaiot-pse84-ai/`, docs J4/J5 | QWA309 base board (quotes schematic V3_P1) |
| SDK web | <https://tesaiot.github.io/tesaiot-pse84-devkit-sdk/> | overview; its "Peripherals at a glance" has errors (pot on P15[1], user button "SW2") |
| Examples | <https://github.com/orgs/Infineon/repositories?q=mtb-example-psoc-edge> | code examples (`examples.sh` lists those for the BSP) |

## Source priority (when they disagree)

The board in front of you (verified test) > schematic > BSP `design.modus` > Infineon UG > TESAIoT
SDK source > SDK web docs > pinout image.

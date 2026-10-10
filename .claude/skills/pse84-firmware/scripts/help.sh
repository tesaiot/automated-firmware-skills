#!/usr/bin/env bash
# Command menu of the pse84-firmware skill: what you can ask Claude, with short examples,
# the script behind each request, and the boards/templates installed right now.
# Usage: help.sh [--en]      (Thai by default)

. "$(dirname "$0")/env.sh"
LANG_EN=0; [ "$1" = "--en" ] && LANG_EN=1

boards() {
  for f in "$PACKS_ROOT"/*/boards/*/board.env; do
    id="$(basename "$(dirname "$f")")"; [ "${id#_}" = "$id" ] || continue
    name="$(sed -n 's/^BOARD_NAME="\{0,1\}\([^"#]*\)"\{0,1\}.*/\1/p' "$f" | head -1)"
    ext="$(sed -n 's/^BOARD_EXTENDS=\([A-Za-z0-9_-]*\).*/\1/p' "$f")"
    pack="$(basename "$(dirname "$(dirname "$(dirname "$f")")")")"
    printf '    %-14s %s%s  [pack %s]\n' "$id" "$name" "${ext:+  (= $ext + extras)}" "$pack"
  done
}
templates() {
  for d in "$SKILL_DIR"/templates/*/; do
    t="$(basename "$d")"; [ "${t#_}" = "$t" ] || continue
    desc="$(head -1 "$d/description.txt" 2>/dev/null | tr -d '\r')"
    printf '    %-14s %s\n' "$t" "$desc"
  done
}

if [ $LANG_EN = 1 ]; then cat <<'EOF'
PSOC Edge E84 firmware skill - what you can ask (say it in your own words)

 Setup
  "check my tools"                         -> check_tools.sh <board>
  "find the board(s) I plugged in"         -> discover.sh <board> [--serial <probe>]
 Explore
  "list ModusToolbox examples for tesaiot" -> examples.sh <board> [words]
  "examples about bluetooth / wifi / ml"   -> examples.sh <board> ble | --category Wi-Fi
  "tell me about btstack-findme"           -> examples.sh <board> --detail <id>
 Create + build
  "create a hello-world app for tesaiot"   -> new_app.sh <board> <app> "" hello-world
  "start from the BLE Find Me example"     -> new_app.sh <board> <app> --example btstack-findme
  "write an app: press SW1, LED1 lights"   -> template + code in board/ func/ main.c layers
  "which components exist?"               -> components.py list [--all] | scopes | check
  "FreeRTOS on both cores" / "bare metal"  -> new_app.sh <board> <app> "" <template> --scope dual --model cm33=freertos,cm55=freertos
  "open the wizard" / "ask me in a form"   -> web-wizard skill: wizard.py project --board <b>  (one browser form)
  "build it"                               -> build.sh <app>
  "open it in VS Code"                     -> open_ide.sh <app> [--refresh]
 Run on hardware (Claude asks before every flash)
  "back up the board first"                -> backup.sh <board>
  "flash it" / "restore the backup"        -> flash.sh <app> --yes [--hex <file>]
  "test it" / "test with button presses"   -> serial_test.py auto <spec> <log> --board <b> [--interactive]
  "debug the sensors" / "why is MAG ERR?"  -> sensor-debug skill: sensor_debug.py --board <b> diag | watch | diff
  "show the readings live in the browser"  -> sensor-debug skill: sensor_dash.py --app <app>  (stop it after)
 Housekeeping
  "clean the workspace"                    -> clean.sh [--build-only]  (dry run, then --yes)
  "add a new board" / "add a template"     -> boards/_template/README.md, SKILL.md "Extending"
EOF
else cat <<'EOF'
skill pse84-firmware (PSOC Edge E84) - สั่ง Claude ได้ด้วยภาษาของคุณเอง ตัวอย่าง:

 เตรียมเครื่อง
  "ตรวจเครื่องมือให้หน่อย"                  -> check_tools.sh <board>
  "หาบอร์ดที่เสียบอยู่"                       -> discover.sh <board> [--serial <probe>]
 ดูตัวอย่างของ ModusToolbox
  "ขอรายการ example ของ tesaiot"            -> examples.sh <board> [คำค้น]
  "มี example เกี่ยวกับ bluetooth ไหม"       -> examples.sh <board> ble   หรือ --category Wi-Fi
  "อธิบาย btstack-findme หน่อย"              -> examples.sh <board> --detail <id>
 สร้างแอป + build
  "สร้างแอป hello world ให้ tesaiot"          -> new_app.sh <board> <app> "" hello-world
  "เริ่มจาก example BLE Find Me"             -> new_app.sh <board> <app> --example btstack-findme
  "เขียนแอป กด SW1 แล้ว LED1 ติด"             -> template + โค้ดแบ่ง layer board/ func/ main.c
  "มี component อะไรบ้าง"                     -> components.py list [--all] | scopes | check
  "ใช้ FreeRTOS ทั้งสอง core"                    -> new_app.sh ... --scope dual --model cm33=freertos,cm55=freertos
  "เปิดหน้าเว็บให้เลือก" / "ถามผ่านฟอร์ม"          -> web-wizard: wizard.py project --board <b>
  "build ให้หน่อย"                           -> build.sh <app>
  "เปิดใน VS Code"                           -> open_ide.sh <app> [--refresh]
 ใช้กับบอร์ดจริง (Claude จะขออนุญาตก่อน flash ทุกครั้ง)
  "สำรอง firmware เดิมก่อน"                   -> backup.sh <board>
  "flash เลย" / "คืน firmware เดิม"           -> flash.sh <app> --yes [--hex <file>]
  "ทดสอบ" / "ทดสอบแบบกดปุ่มด้วย"             -> serial_test.py auto <spec> <log> --board <b> [--interactive]
  "ดีบักเซนเซอร์" / "ค่าเซนเซอร์ผิด"              -> sensor-debug: sensor_debug.py --board <b> diag | watch | diff
  "ดูค่าเซนเซอร์สดบนเว็บ" / "เปิดกราฟ"             -> sensor-debug: sensor_dash.py --app <app>  (ปิดเมื่อใช้เสร็จ)
 ดูแลโฟลเดอร์
  "clean ให้หน่อย"                            -> clean.sh [--build-only]  (แสดงรายการก่อน แล้วค่อย --yes)
  "เพิ่มบอร์ดใหม่" / "เพิ่ม template"           -> boards/_template/README.md, SKILL.md หัวข้อ Extending
EOF
fi
echo
[ $LANG_EN = 1 ] && echo " Boards installed:" || echo " บอร์ดที่มี profile:"
boards
[ $LANG_EN = 1 ] && echo " App templates:" || echo " template แอป:"
templates
[ $LANG_EN = 1 ] && echo " Components (bsp / func / exec):" || echo " component (bsp / func / exec):"
"$PYTHON" "$SKILL_DIR/scripts/components.py" list | cut -c1-110 | sed 's/^/    /'
[ $LANG_EN = 1 ] && echo " Apps workspace: $PSE84_WS" || echo " โฟลเดอร์แอป: $PSE84_WS"

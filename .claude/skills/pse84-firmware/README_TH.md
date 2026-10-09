# pse84-firmware — คู่มือภาษาไทย

Skill สำหรับ Claude Code ใช้สร้าง firmware บอร์ดตระกูล **Infineon PSOC Edge E84** ด้วย
ModusToolbox แบบอัตโนมัติ ตั้งแต่ตรวจเครื่องมือจนถึงทดสอบบนบอร์ดจริง
(คำสั่งหลักสำหรับ Claude อยู่ใน `SKILL.md` เป็นภาษาอังกฤษ)

## ⚠️ ก่อนเริ่ม: ชื่อโฟลเดอร์

ModusToolbox (make, configurator, OpenOCD) **ทำงานผิดพลาดถ้า path มีช่องว่าง หรือมีตัวอักษรที่ไม่ใช่ภาษาอังกฤษ (เช่น ภาษาไทย)**
ทั้งโฟลเดอร์ workspace/แอป, โฟลเดอร์ผู้ใช้ Windows (`C:\Users\<ชื่อ>`) และโฟลเดอร์ติดตั้ง ModusToolbox

- ใช้เฉพาะ A-Z, a-z, 0-9, `-`, `_` เช่น `C:\Users\<you>\Dropbox\works\TESAIoT_track`
- ถ้าชื่อผู้ใช้ Windows เป็นภาษาไทย ให้วาง workspace นอกโฟลเดอร์ผู้ใช้ เช่น `C:\pse84-ws`
- โฟลเดอร์ที่ sync (Dropbox/OneDrive) ใช้ได้ แต่ถ้า build ล้มเพราะไฟล์ถูกล็อก ให้หยุด sync ชั่วคราว
- ขั้นที่ 1 (`check_tools.sh`) ตรวจให้อัตโนมัติ ถ้าขึ้น `BAD` ต้องแก้ก่อน และ `new_app` / `build` / `flash` จะไม่ยอมทำงานบน path ที่ไม่ปลอดภัย

## โครงสร้างโฟลเดอร์ template ของกลุ่ม

```
TESAIoT_track/                       ← โฟลเดอร์ template (เปิด Claude Code ที่นี่)
├── .claude/skills/pse84-firmware/   ← skill (เก็บไว้เสมอ)
└── apps/                            ← แอปที่สร้างขึ้น + mtb_shared (ลบได้ด้วยขั้น clean)
```

## ลำดับงาน 6 ขั้น + clean (แต่ละขั้นมี "gate" ต้องผ่านก่อนไปขั้นถัดไป)

| ขั้น | ทำอะไร | สคริปต์ | ผ่านเมื่อ |
| --- | --- | --- | --- |
| 1 | ตรวจชื่อโฟลเดอร์ + dev tools (MTB, ProgTools, GCC, Edge Protect, python/pyserial) | `scripts/check_tools.sh <board>` | `missing/bad=0` |
| 2a | ค้นหาบอร์ดที่ต่อกับเครื่องนี้ (อ่านอย่างเดียว) แล้วบันทึก probe serial + COM port ของเครื่อง | `discover.sh <board>` | `IDENTITY: PASS`, `Saved ...` |
| 2b | สร้าง project ด้วย Project Creator + overlay BSP (ไม่ต้องมีบอร์ด) | `new_app.sh <board> <app>` | `Created ...` |
| 3 | เขียนโค้ดแยก layer: `board/` → `func/` → `main.c` และเขียน test spec จากโจทย์ | `templates/push-to-light/` | โค้ดไม่ข้าม layer |
| 4 | build ทั้ง 3 core + sign + combined hex | `build.sh <app>` | `BUILD: PASS`, 0 warning |
| 5 | flash (ต้องขออนุญาตผู้ใช้ก่อนทุกครั้ง) ตรวจ manifest, ตัวตนบอร์ด และ life cycle ก่อนเขียน | `flash.sh <app> --yes` | `FLASH: PASS` (exit 3 = ให้ถอดเสียบ USB) |
| 6 | ทดสอบผ่าน UART ตาม spec JSON (รันหลัง flash) | `serial_test.py auto <spec> <log> --board <board>` | `RESULT: PASS` |
| 7 | clean: ลบแอปและไฟล์ที่สร้างขึ้น คงไว้เฉพาะ skill เพื่อใช้เป็น template สร้างแอปใหม่ | `clean.sh` (dry run) แล้ว `clean.sh --yes` | `CLEAN: done` |

ขั้น clean มี 2 แบบ:
- **ค่าปกติ:** ลบทุกแอปที่สร้างด้วย `new_app.sh` (โฟลเดอร์ที่มี `pse84-app.env`) + `apps/mtb_shared` + cache + `apps/.bench` (ข้อมูลบอร์ดของเครื่องนี้ สร้างใหม่ได้ด้วย `discover.sh`)
- **`--build-only`:** เก็บ source/test/log ไว้ ลบเฉพาะผล build และ library (ดึงใหม่ได้ด้วย `build.sh <app> --getlibs`)

ถ้าไม่ใส่ `--yes` จะแสดงรายการที่จะลบเท่านั้น ควรสำรอง log หรือโค้ดที่ต้องการเก็บก่อนลบจริง
ถ้าขึ้น `CLEAN: INCOMPLETE` แปลว่า Dropbox/OneDrive ล็อกไฟล์อยู่ ให้หยุด sync ชั่วคราวแล้วรันซ้ำ

## ข้อมูลบอร์ด 2 ระดับ

- **ชนิดบอร์ด** (`boards/<id>/board.env` ใน skill) — เหมือนกันทุกบอร์ดรุ่นเดียวกัน: BSP, overlay, ชิปที่คาดหวัง, ชื่อปุ่ม
- **บอร์ดที่ต่อกับเครื่องนี้** (`apps/.bench/<id>.env`) — probe serial และ COM port ซึ่งต่างกันทุกเครื่อง
  นักศึกษาแต่ละคนรัน `discover.sh <board>` ครั้งแรกครั้งเดียว ห้ามแก้ค่าเหล่านี้ใน `boards/`

## โครงสร้างโฟลเดอร์ skill

```
SKILL.md                 คำสั่งสำหรับ Claude (อังกฤษ)
boards/<id>/             ข้อมูลเฉพาะบอร์ด: board.env (ให้สคริปต์อ่าน), README.md, overlay
reference/               รายละเอียดแต่ละขั้น, ปัญหาที่เคยเจอและวิธีแก้
scripts/                 สคริปต์ทุกขั้น (รันด้วย Git Bash)
templates/<ชื่อ>/        template แอปแบบแบ่ง layer พร้อม test spec: hello-world, blink, push-to-light, button-led,
                         hello-world, dual-core-ipc (CM33 console + CM55 ทำงานผ่าน mailbox)
```

## เมนูวิธีใช้ และ example ของ ModusToolbox

- พิมพ์ **"ขอวิธีใช้หน่อย"** หรือ "มีคำสั่งอะไรบ้าง" — Claude จะแสดงรายการคำสั่งพร้อมตัวอย่าง (มาจาก `scripts/help.sh`)
- **"ขอรายการ example ของ tesaiot"** / **"มี example bluetooth ไหม"** — ดึงรายการ code example ของ Infineon ที่รองรับบอร์ดนั้น (`examples.sh`)
- **"เริ่มจาก example btstack-findme"** — สร้างแอปจาก example โดยล็อกเวอร์ชันทั้ง example และ BSP (`new_app.sh ... --example <id>`)
  example ที่ปรับ BSP เอง (เช่น Bluetooth) ใช้กับบอร์ดที่มี BSP overlay ไม่ได้โดยตรง ต้องรวมการตั้งค่าเอง

## ใช้หลายบอร์ดในเครื่องเดียว

เสียบหลายบอร์ดได้ แต่ต้องรัน `discover.sh <board> --serial <probe>` แยกให้แต่ละบอร์ด (สคริปต์จะไม่เดาให้)
เพราะทุกบอร์ดใช้ชิปรุ่นเดียวกัน สิ่งเดียวที่แยกบอร์ดได้คือ serial ของ probe — serial หนึ่งผูกได้กับบอร์ดเดียว

## ก่อนส่งโฟลเดอร์ให้คนอื่น

1. `clean.sh --yes` เพื่อลบแอป, library และข้อมูล probe/COM ของเครื่องนี้ (`apps/`)
   (ส่งเฉพาะโฟลเดอร์ `pse84-firmware/` ก็ได้ ผู้รับต้องวางไว้ที่ `<project>/.claude/skills/` หรือ `~/.claude/skills/` แอปจะไปอยู่ที่ `<project>/apps` หรือ `./apps` ตามลำดับ)
2. ย้ายโฟลเดอร์ `backup/` ออก (เป็นข้อมูล firmware ของบอร์ดเราเอง)
3. ผู้รับเปิด Claude Code ในโฟลเดอร์ แล้วเริ่มจากขั้นที่ 1 (`check_tools.sh`) และ `discover.sh`

## การแบ่ง layer ของโค้ด

- **board/** — ชั้นเดียวที่รู้จักฮาร์ดแวร์ (`CYBSP_*`, `Cy_*`) เปลี่ยนบอร์ดแก้ที่นี่ (หรือที่ BSP)
- **func/** — บริการที่ใช้ซ้ำได้ เช่น led, button (debounce), console (รับคำสั่ง)
- **main.c** — ประกอบ function เข้าด้วยกันเป็นแอป

## คำสั่งของแอปตัวอย่าง (115200 8N1)

`help`, `info`, `led <n> on|off|toggle`, `led?`, `btn?` — กดปุ่ม BTNn จะสลับ LEDn และพิมพ์ `EVT ...`

## บอร์ดที่รองรับ

| บอร์ด | สถานะ |
| --- | --- |
| KIT_PSE84_AI (AI Kit) | ทดสอบบนบอร์ดจริงแล้ว (ผ่าน profile tesaiot) |
| TESAIoT DevKit | ทดสอบบนบอร์ดจริงแล้ว (milestone 2) — ปุ่ม BTN1 คือ SW1 บนบอร์ด AI Kit (SW2 = RESET); ปุ่มบนบอร์ดเสริม QWA309 คือ SW4/SW5 |
| บอร์ด E84 อื่น | ทำตาม `boards/_template/README.md` — milestone 3 |

## ตัวอย่างการสั่ง Claude

> "ใช้ skill pse84-firmware สร้างแอป push-to-light สำหรับบอร์ด tesaiot แล้ว build, flash และทดสอบ"

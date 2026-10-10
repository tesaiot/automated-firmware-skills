# Common environment for pse84-firmware scripts. Source it: . "$(dirname "$0")/env.sh"
# Windows + Git Bash only (uses cygpath and the .exe tools of ModusToolbox for Windows).
# Run scripts from Git Bash or Claude's Bash tool - in PowerShell, "bash" may start WSL instead.

SKILL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BOARDS_DIR="$SKILL_DIR/boards"                 # engine: only _template - real boards live in hardware packs
PACKS_ROOT="$(dirname "$SKILL_DIR")"           # sibling skill folders with a pack.json (scripts/packs.py)
# board_dir <id>: profile folder of a board (engine boards/ first, then the boards/ of every pack)
board_dir() {
  local d
  [ -f "$BOARDS_DIR/$1/board.env" ] && { echo "$BOARDS_DIR/$1"; return 0; }
  for d in "$PACKS_ROOT"/*/boards/"$1"; do
    [ -f "$d/board.env" ] && [ -f "$d/../../pack.json" ] && { echo "$d"; return 0; }
  done
  return 1
}
# board_ids: every real board id in the packs
board_ids() { for d in "$PACKS_ROOT"/*/boards/*/; do basename "$d"; done | grep -v '^_' | sort -u | tr '\n' ' '; }

# Where apps go (PSE84_WS), decided from where the skill folder sits:
#   <project>/.claude/skills/pse84-firmware  -> <project>/apps      (project skill, "template root")
#   ~/.claude/skills/pse84-firmware          -> ./apps             (user skill: apps next to cwd)
#   anywhere else (copied folder)            -> ./apps
# Set PSE84_WS to override. Backups go next to the workspace unless PSE84_BACKUP_DIR is set.
TEMPLATE_ROOT=""
if [ "$(basename "$(dirname "$SKILL_DIR")")" = skills ] && \
   [ "$(basename "$(dirname "$(dirname "$SKILL_DIR")")")" = .claude ]; then
  TEMPLATE_ROOT="$(cd "$SKILL_DIR/../../.." && pwd)"
  [ "$TEMPLATE_ROOT" = "$(cd "$HOME" && pwd)" ] && TEMPLATE_ROOT=""   # user-level skill
fi
if [ -z "$PSE84_WS" ]; then
  if [ -n "$TEMPLATE_ROOT" ]; then PSE84_WS="$TEMPLATE_ROOT/apps"; else PSE84_WS="$PWD/apps"; fi
fi
PSE84_BACKUP_DIR="${PSE84_BACKUP_DIR:-$(dirname "$PSE84_WS")/backup}"
# Bench instance files (probe serial, COM port of THIS PC's board) - written by discover.sh
BENCH_DIR="${PSE84_BENCH_DIR:-$PSE84_WS/.bench}"

# Newest installed ModusToolbox tools_X.Y unless MTB_TOOLS_DIR is preset
if [ -z "$MTB_TOOLS_DIR" ]; then
  MTB_TOOLS_DIR="$(ls -d "$HOME"/ModusToolbox/tools_* 2>/dev/null | sort -V | tail -1)"
fi
if [ -z "$PROGTOOLS_DIR" ]; then
  PROGTOOLS_DIR="$(ls -d /c/Infineon/Tools/ModusToolboxProgtools-* "$HOME"/Infineon/Tools/ModusToolboxProgtools-* 2>/dev/null | sort -V | tail -1)"
fi
GCC_DIR="$(ls -d "$HOME"/Infineon/Tools/mtb-gcc-arm-eabi/*/ /c/Infineon/Tools/mtb-gcc-arm-eabi/*/ 2>/dev/null | sort -V | tail -1)"
GCC_DIR="${GCC_DIR%/}"
[ -d "$GCC_DIR/gcc/bin" ] && GCC_DIR="$GCC_DIR/gcc"   # Infineon installer layout: <ver>/gcc/bin
EDGEPROTECT_DIR="$(ls -d "$HOME"/Infineon/Tools/ModusToolbox-Edge-Protect-Security-Suite-* /c/Infineon/Tools/ModusToolbox-Edge-Protect-Security-Suite-* 2>/dev/null | sort -V | tail -1)"

MODUS_BASH="$MTB_TOOLS_DIR/modus-shell/bin/bash.exe"
PROJECT_CREATOR="$MTB_TOOLS_DIR/project-creator/project-creator-cli.exe"
DEVICE_CONFIGURATOR="$MTB_TOOLS_DIR/device-configurator/device-configurator-cli.exe"
QSPI_CONFIGURATOR="$MTB_TOOLS_DIR/qspi-configurator/qspi-configurator-cli.exe"
OPENOCD_DIR="$PROGTOOLS_DIR/openocd"
OPENOCD="$OPENOCD_DIR/bin/openocd.exe"
FW_LOADER="$PROGTOOLS_DIR/fw-loader/bin/fw-loader.exe"
PYTHON="${PYTHON:-$(command -v python || command -v python3)}"
# OpenOCD must not grab fixed TCP ports (several boards / an open IDE debug session)
OPENOCD_NO_PORTS="gdb_port disabled; telnet_port disabled; tcl_port disabled"

die()  { echo "ERROR: $*" >&2; exit 1; }
info() { echo "==> $*"; }

# path_problem <path>: print why a path is unsafe for ModusToolbox make/tools, or nothing.
# GNU make (core-make) and several MTB tools break on spaces; non-ASCII (e.g. Thai)
# characters break make, the configurators and OpenOCD path handling.
path_problem() {
  local p="$1" why=""
  case "$p" in *" "*) why="contains a space";; esac
  case "$p" in *"'"*|*'"'*) why="${why:+$why, }contains a quote";; esac
  if LC_ALL=C printf '%s' "$p" | LC_ALL=C grep -q '[^ -~]'; then
    why="${why:+$why, }contains non-ASCII (non-English) characters"
  fi
  [ -n "$why" ] && echo "$why"
}

# path_synced <path>: print the sync service if the path is inside a cloud-synced folder
path_synced() {
  case "$(echo "$1" | tr 'A-Z' 'a-z')" in
    *dropbox*) echo Dropbox;; *onedrive*) echo OneDrive;; *"google drive"*|*googledrive*) echo "Google Drive";;
  esac
}

# require_safe_path <path> <what>: die if the path would break the build
require_safe_path() {
  local why; why="$(path_problem "$1")"
  [ -z "$why" ] || die "$2 path '$1' $why. Use a folder with only English letters, digits, - and _ (no spaces), e.g. C:/pse84-ws"
}

# require_vars VAR...: die if any profile/bench variable is empty
require_vars() {
  local v empty=""
  for v in "$@"; do [ -n "${!v}" ] || empty="$empty $v"; done
  [ -z "$empty" ] && return 0
  die "board '$BOARD_ID' has no value for:$empty
       Board-type values go in $BOARD_DIR/board.env; this PC's probe/COM port come from
       'discover.sh $BOARD_ID' (writes $BENCH_DIR/$BOARD_ID.env)."
}

# load_board <id>: board type (skill) -> bench instance (this PC) -> env overrides
load_board() {
  BOARD_DIR="$(board_dir "$1")" || die "unknown board '$1'. Known: $(board_ids)"
  # BOARD_EXTENDS=<parent>: load the parent profile first, then override with this one.
  # Overlay files are looked up here first, then in BOARD_PARENT_DIR.
  BOARD_PARENT_DIR=""
  local parent; parent="$(sed -n 's/^BOARD_EXTENDS=\([A-Za-z0-9_-]*\).*/\1/p' "$BOARD_DIR/board.env")"
  if [ -n "$parent" ]; then
    BOARD_PARENT_DIR="$(board_dir "$parent")" || die "board '$1' extends unknown board '$parent'"
    # shellcheck disable=SC1090
    . "$BOARD_PARENT_DIR/board.env"
  fi
  # shellcheck disable=SC1090
  . "$BOARD_DIR/board.env"
  BENCH_FILE="$BENCH_DIR/$1.env"
  # shellcheck disable=SC1090
  [ -f "$BENCH_FILE" ] && . "$BENCH_FILE"
  [ -n "$PSE84_PROBE_SERIAL" ] && PROBE_SERIAL="$PSE84_PROBE_SERIAL"
  [ -n "$PSE84_CONSOLE" ] && CONSOLE_PORT="$PSE84_CONSOLE"
  return 0
}

# mtb_make <app_dir> <make args...>: run make inside modus-shell with the pinned tools dir
mtb_make() {
  local winapp; winapp="$(cygpath -m "$1")"; shift
  local tools; tools="$(cygpath -m "$MTB_TOOLS_DIR")"
  "$MODUS_BASH" --login -c "cd \"\$(cygpath -u '$winapp')\" && make $* CY_TOOLS_PATHS='$tools'"
}

# find_console_port <probe_serial>: print the COM port of that KitProg3's USB-UART (or nothing)
find_console_port() {
  [ -n "$1" ] || return 0
  "$PYTHON" - "$1" <<'EOF'
import sys
from serial.tools import list_ports
want = sys.argv[1].upper()
for p in list_ports.comports():
    if p.vid == 0x04B4 and (p.serial_number or "").upper() == want:
        print(p.device); break
EOF
}

# resolve_console: set CONSOLE (auto-detect by probe serial, fallback to bench CONSOLE_PORT)
resolve_console() {
  CONSOLE="$(find_console_port "$PROBE_SERIAL" 2>/dev/null | tr -d '\r')"
  [ -n "$CONSOLE" ] || CONSOLE="$CONSOLE_PORT"
}

ts() { date +%Y%m%d-%H%M%S; }

# --- Stage 2d: VS Code + Infineon ModusToolbox for VS Code (open_ide.sh) ---
IDE_EXT=infineonag.modustoolbox-for-vscode
IDE_NAME="Infineon ModusToolbox for VS Code"
winpath() { if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else echo "$1"; fi; }
# need_user <TYPE> <message>: stop and hand over to the developer (exit 10, "ACTION:" line)
need_user() { local type="$1"; shift; echo "ACTION: $type $*"; exit 10; }
fw_code_cli() {
  local c la; la="$(cygpath -u "${LOCALAPPDATA:-}" 2>/dev/null)"
  for c in "$(command -v code 2>/dev/null)" "$la/Programs/Microsoft VS Code/bin/code" \
           "/c/Program Files/Microsoft VS Code/bin/code"; do
    [ -n "$c" ] && [ -f "$c" ] && { echo "$c"; return 0; }
  done
  return 1
}
# version of IDE_EXT installed in VS Code (exit 1 if missing); whole list read first, lowercase compare
fw_ide_ext_version() {
  local list want
  list="$("$1" --list-extensions --show-versions 2>/dev/null | tr -d '\r' | tr 'A-Z' 'a-z')"
  want="$(echo "$IDE_EXT" | tr 'A-Z' 'a-z')"
  printf '%s\n' "$list" | grep -F "$want@" | grep "^$want@" | head -1 | sed 's/.*@//' | grep .
}
# fw_open_ide <workspace-file> [0]: gate "IDE: READY", check VS Code + extension, open the
# workspace (never the app folder). Second argument 0 = check only.
fw_open_ide() {
  local ws="$1" open="${2:-1}" code
  [ -f "$ws" ] || die "no $ws - re-run open_ide.sh"
  echo "IDE: READY $(winpath "$ws")  (VS Code + $IDE_NAME)"
  code="$(fw_code_cli)" || need_user SETUP "install Visual Studio Code and its extension $IDE_NAME ($IDE_EXT) - or open $(winpath "$ws") yourself with File > Open Workspace from File"
  fw_ide_ext_version "$code" >/dev/null \
    || need_user SETUP "install the VS Code extension $IDE_NAME: code --install-extension $IDE_EXT, then re-run open_ide.sh"
  [ "$open" = 1 ] || { echo "IDE: not opened (--no-open)"; return 0; }
  # not backgrounded: the code CLI returns once VS Code has the window
  "$code" "$(winpath "$ws")" </dev/null >/dev/null 2>&1 \
    || need_user SETUP "VS Code did not start - open $(winpath "$ws") yourself with File > Open Workspace from File"
  echo "IDE: OPENED in VS Code"
}

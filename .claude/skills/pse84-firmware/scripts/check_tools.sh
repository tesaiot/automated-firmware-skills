#!/usr/bin/env bash
# Step 1: check the development tools and folder paths needed for PSOC Edge E84 firmware.
# Usage: check_tools.sh [board-id] [--fix] [--ws <workspace-dir>]
#   board-id  compare installed versions against the board profile
#   --fix     install what can be installed safely without admin rights (python pyserial)
#   --ws      workspace where apps are created (default: $PSE84_WS or ./apps)
# Exit 0 = all required tools present and paths safe; 1 = something must be fixed first.

. "$(dirname "$0")/env.sh"
BOARD=""; FIX=0; WS="${PSE84_WS:-apps}"
while [ $# -gt 0 ]; do
  case "$1" in --fix) FIX=1;; --ws) WS="$2"; shift;; *) BOARD="$1";; esac; shift
done
[ -n "$BOARD" ] && load_board "$BOARD"

missing=0; warn=0
row() { printf '%-8s %-28s %s\n' "$1" "$2" "$3"; }
ok()   { row "OK" "$1" "$2"; }
miss() { row "MISSING" "$1" "$2"; missing=$((missing+1)); }
bad()  { row "BAD" "$1" "$2"; missing=$((missing+1)); }
wrn()  { row "WARN" "$1" "$2"; warn=$((warn+1)); }

echo "PSOC Edge E84 toolchain check  ($(date '+%Y-%m-%d %H:%M'))"
echo "----------------------------------------------------------------"

# Paths: spaces or non-English characters break ModusToolbox make/tools -> must fix first
WSABS="$(mkdir -p "$WS" 2>/dev/null; cd "$WS" 2>/dev/null && pwd || echo "$WS")"
for item in "workspace|$WSABS" "home folder|$HOME" "ModusToolbox tools|$MTB_TOOLS_DIR"; do
  name="${item%%|*}"; p="${item#*|}"; [ -n "$p" ] || continue
  why="$(path_problem "$p")"
  if [ -n "$why" ]; then
    bad "path: $name" "$p -> $why. Use only English letters, digits, - and _ (e.g. C:/pse84-ws)"
  else
    ok "path: $name" "$p"
  fi
done
svc="$(path_synced "$WSABS")"
[ -n "$svc" ] && wrn "path: workspace sync" "$svc folder: pause sync during getlibs/build or move mtb_shared/build outside (file locks, slow builds)"

# ModusToolbox tools package
if [ -d "$MTB_TOOLS_DIR" ]; then
  v="$(basename "$MTB_TOOLS_DIR" | sed 's/tools_//')"
  ok "ModusToolbox tools" "$v  ($MTB_TOOLS_DIR)"
  if [ -n "$MTB_TOOLS_VERSION" ] && [ "$v" != "$MTB_TOOLS_VERSION" ]; then
    wrn "MTB version vs profile" "profile proven with $MTB_TOOLS_VERSION, found $v"
  fi
else
  miss "ModusToolbox tools" "install ModusToolbox Setup: https://www.infineon.com/modustoolbox"
fi
for t in "$PROJECT_CREATOR|project-creator-cli" "$DEVICE_CONFIGURATOR|device-configurator-cli" "$MODUS_BASH|modus-shell"; do
  f="${t%%|*}"; n="${t##*|}"
  [ -f "$f" ] && ok "$n" "" || miss "$n" "part of ModusToolbox tools package"
done

# Programming tools (OpenOCD, fw-loader)
if [ -x "$OPENOCD" ]; then
  ov="$("$OPENOCD" --version 2>&1 | head -1 | sed 's/.*Open On-Chip Debugger //')"
  ok "Programming Tools" "$(basename "$PROGTOOLS_DIR")  openocd $ov"
else
  miss "Programming Tools" "install ModusToolbox Programming Tools (Setup program) - provides openocd"
fi
[ -x "$FW_LOADER" ] && ok "fw-loader" "" || wrn "fw-loader" "not found (needed for probe discovery / KitProg3 firmware update)"

# GCC
if [ -n "$GCC_DIR" ] && { [ -x "$GCC_DIR/bin/arm-none-eabi-gcc.exe" ] || [ -x "$GCC_DIR/bin/arm-none-eabi-gcc" ]; }; then
  gv="$("$GCC_DIR/bin/arm-none-eabi-gcc" -dumpversion 2>/dev/null)"
  ok "Arm GNU toolchain" "$gv  ($GCC_DIR)"
else
  miss "Arm GNU toolchain" "install 'Arm GCC' via ModusToolbox Setup"
fi

# Edge Protect Security Suite (signs the secure M33 image)
if [ -n "$EDGEPROTECT_DIR" ]; then
  ok "Edge Protect Security Suite" "$(basename "$EDGEPROTECT_DIR" | sed 's/.*Suite-//')"
else
  miss "Edge Protect Security Suite" "install via ModusToolbox Setup (needed for postbuild signing)"
fi

# Git, Python, pyserial
command -v git >/dev/null && ok "git" "$(git --version | awk '{print $3}')" || miss "git" "install Git for Windows"
if [ -n "$PYTHON" ] && "$PYTHON" -c 'import sys' 2>/dev/null; then
  ok "python" "$("$PYTHON" -c 'import sys;print(sys.version.split()[0])')"
  if "$PYTHON" -c 'import serial' 2>/dev/null; then
    ok "pyserial" "$("$PYTHON" -c 'import serial;print(serial.__version__)')"
  elif [ $FIX = 1 ]; then
    "$PYTHON" -m pip install --user pyserial >/dev/null && ok "pyserial" "installed now" || miss "pyserial" "pip install pyserial failed"
  else
    miss "pyserial" "run: python -m pip install pyserial  (or re-run with --fix)"
  fi
else
  miss "python" "install Python 3.10+"
fi

# IDE path (stage 2d): VS Code + ModusToolbox extension. WARN only - the script path works without it.
if code="$(fw_code_cli)"; then
  ok "IDE: VS Code" "$("$code" --version 2>/dev/null | head -1 | tr -d '')"
  if v="$(fw_ide_ext_version "$code")"; then ok "IDE: extension" "$IDE_NAME $v"
  else wrn "IDE: extension" "$IDE_NAME missing - code --install-extension $IDE_EXT (IDE path only)"; fi
else
  wrn "IDE: VS Code" "not found - needed only for the IDE path (open_ide.sh)"
fi

# Probe (informational only; no board access beyond USB enumeration)
if [ -n "$PYTHON" ] && "$PYTHON" -c 'import serial' 2>/dev/null; then
  probes="$("$PYTHON" -c 'from serial.tools import list_ports
for p in list_ports.comports():
    if p.vid==0x04B4: print(p.device, p.serial_number, p.description)')"
  [ -n "$probes" ] && ok "KitProg3 USB-UART" "$(echo "$probes" | tr '\n' ';')" || wrn "KitProg3 USB-UART" "no Infineon COM port enumerated (board unplugged?)"
fi

echo "----------------------------------------------------------------"
echo "missing/bad=$missing warnings=$warn"
[ $missing -eq 0 ]

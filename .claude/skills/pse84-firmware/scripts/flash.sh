#!/usr/bin/env bash
# Stage 5: program the combined image to THIS PC's board (probe serial from discover.sh).
# Usage: flash.sh <app-dir> --yes [--board <id>] [--hex <file>]
#   --yes    REQUIRED: records that the user approved erasing/programming this board
#   --board  defaults to APP_BOARD in <app>/pse84-app.env
#   --hex    flash another combined hex (e.g. a known-good/recovery image) instead of the
#            app's build/app_combined.hex; skips the manifest check
# Gates, in order:
#   1. app built for this board (pse84-app.env) and build manifest matches the hex (build PASS)
#   2. read-only identity check (probe serial + device)
#   3. acquire-only attach (resets the chip, writes nothing): device + life cycle must match
#   4. program + verify + reset
# Exit 0 = PASS; 1 = FAIL; 3 = PASS but the user must power-cycle the board now
#   (profile POWER_CYCLE_AFTER_FLASH=1; the COM port disappears while unplugged).
# Start serial_test.py BEFORE this script (for exit 3, start it with --wait-port instead).

. "$(dirname "$0")/env.sh"
APPDIR=""; YES=0; BOARD=""; HEX=""
while [ $# -gt 0 ]; do
  case "$1" in
    --yes) YES=1;; --board) BOARD="$2"; shift;; --hex) HEX="$2"; shift;;
    --*) die "unknown option $1";;
    *) [ -z "$APPDIR" ] && APPDIR="$1" || die "unexpected argument $1";;
  esac; shift
done
[ -d "$APPDIR" ] || die "usage: flash.sh <app-dir> --yes [--board <id>] [--hex <file>]"
APPDIR="$(cd "$APPDIR" && pwd)"
require_safe_path "$APPDIR" "app"
APP_BOARD="$(sed -n 's/^APP_BOARD=//p' "$APPDIR/pse84-app.env" 2>/dev/null)"
[ -n "$BOARD" ] || BOARD="$APP_BOARD"
[ -n "$BOARD" ] || die "board unknown: pass --board <id>"
load_board "$BOARD"
require_vars PROBE_SERIAL EXPECTED_DEVICE EXPECTED_LIFECYCLE OPENOCD_TARGET_CFG
[ $YES = 1 ] || die "refusing to program without --yes (ask the user first: this erases and reprograms the board)"
[ -z "$APP_BOARD" ] || [ "$APP_BOARD" = "$BOARD_ID" ] || die "app was created for '$APP_BOARD', not '$BOARD_ID'"
# hardware settings the app's components need (components.py hw_settings): remind before programming
hw="$(sed -n "s/^APP_HW_SETTINGS='\(.*\)'$/\1/p" "$APPDIR/pse84-app.env" 2>/dev/null)"
[ -n "$hw" ] && printf '%s\n' "$hw" | tr '|' '\n' | sed 's/^ *//; s/^/CHECK HW: /'

# 1. image gate
if [ -n "$HEX" ]; then
  [ -f "$HEX" ] || die "no such hex $HEX"; HEX="$(cd "$(dirname "$HEX")" && pwd)/$(basename "$HEX")"
  echo "NOTE: flashing explicit image $HEX (no build manifest check)"
else
  HEX="$APPDIR/build/app_combined.hex"; M="$APPDIR/build/manifest.txt"
  [ -f "$HEX" ] || die "no $HEX - run build.sh first"
  [ -f "$M" ] || die "no build manifest - last build did not PASS; run build.sh"
  grep -q "$(sha256sum "$HEX" | cut -d' ' -f1) build/app_combined.hex" "$M" \
    || die "app_combined.hex does not match build/manifest.txt (stale or modified image) - rebuild"
fi
require_safe_path "$HEX" "hex"
HEXSHA="$(sha256sum "$HEX" | cut -d' ' -f1)"

mkdir -p "$APPDIR/logs"; STAMP="$(ts)"; LOG="$APPDIR/logs/flash-$STAMP.log"
# 2. read-only identity
bash "$SKILL_DIR/scripts/identity_check.sh" "$BOARD_ID" "$APPDIR/logs/identity-$STAMP.log" || die "identity gate failed - not flashing"

cd "$APPDIR/proj_cm33_s" || die "no proj_cm33_s in $APPDIR"
GEN="../bsps/$(basename "$(ls -d ../bsps/*/ | head -1)")/config/GeneratedSource"
OCD=( "$OPENOCD" -s "$(cygpath -m "$OPENOCD_DIR/scripts")" -s "$GEN"
      -c "set QSPI_FLASHLOADER $GEN/PSE84_SMIF.FLM"
      -c "set DEBUG_CERTIFICATE ../packets/debug_token.bin"
      -c "source [find interface/kitprog3.cfg]; adapter serial $PROBE_SERIAL; transport select swd; source [find $OPENOCD_TARGET_CFG]; adapter speed 12000"
      -c "$OPENOCD_NO_PORTS" )

# 3. acquire-only: life cycle is only reported with test-mode acquire. Resets, writes nothing.
info "Acquire check (resets the chip, no write)"
"${OCD[@]}" -c "init; reset init; shutdown" >"$APPDIR/logs/acquire-$STAMP.log" 2>&1
adev="$(grep -m1 'Detected Device:' "$APPDIR/logs/acquire-$STAMP.log" | sed 's/.*Device: *//' | tr -d '\r')"
alcs="$(grep -m1 'Life Cycle' "$APPDIR/logs/acquire-$STAMP.log" | sed 's/.*: *//' | tr -d '\r')"
echo "acquire: device=${adev:-?} lifecycle=${alcs:-?}"
[ "$adev" = "$EXPECTED_DEVICE" ] || die "acquire: device '${adev:-?}' != '$EXPECTED_DEVICE' (logs/acquire-$STAMP.log)"
[ "$alcs" = "$EXPECTED_LIFECYCLE" ] || die "acquire: life cycle '${alcs:-not reported}' != '$EXPECTED_LIFECYCLE' - stop, do not flash; report to the user"

# 4. program (same sequence as 'make qprogram' of recipe mtb-dsl-pse8xxgp, plus adapter serial)
HEXW="$(cygpath -m "$HEX")"
info "Programming $(basename "$HEX") sha256 ${HEXSHA:0:16}... (log $LOG)"
"${OCD[@]}" -c "init; reset init; flash write_image erase {$HEXW}; verify_image {$HEXW}; reset run; shutdown" >"$LOG" 2>&1
rc=$?
dev="$(grep -m1 'Detected Device:' "$LOG" | sed 's/.*Device: *//' | tr -d '\r')"
wrote="$(grep -m1 '^wrote ' "$LOG")"; ver="$(grep -m1 '^verified ' "$LOG")"
echo "device=$dev openocd_exit=$rc hex_sha256=$HEXSHA"
echo "$wrote"; echo "$ver"
if [ $rc -eq 0 ] && [ -n "$ver" ] && [ "$dev" = "$EXPECTED_DEVICE" ]; then
  echo "FLASH: PASS (programmed + verified + reset run)"
  if [ "$POWER_CYCLE_AFTER_FLASH" = 1 ]; then
    echo "ACTION: POWER_CYCLE - ask the user to unplug and replug the board USB now"; exit 3
  fi
  exit 0
fi
grep -iE 'error|fail' "$LOG" | head -15
echo "FLASH: FAIL (log $LOG)"; exit 1

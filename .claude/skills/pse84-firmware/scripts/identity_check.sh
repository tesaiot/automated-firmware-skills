#!/usr/bin/env bash
# Stage 2a / 5 gate: READ-ONLY identity check of probe + chip. No erase, program, reset or halt.
# Usage: identity_check.sh <board-id> [logfile]
# Probe serial comes from the bench file (discover.sh) or PSE84_PROBE_SERIAL.
# Exit 0 = PASS (probe serial and detected device match; life cycle too if reported).

. "$(dirname "$0")/env.sh"
[ -n "$1" ] || die "usage: identity_check.sh <board-id> [logfile]"
load_board "$1"
require_vars PROBE_SERIAL OPENOCD_TARGET_CFG
LOG="${2:-${TMPDIR:-/tmp}/identity-$BOARD_ID-$(ts).log}"
[ -x "$OPENOCD" ] || die "openocd not found ($OPENOCD); run check_tools.sh"

info "Read-only attach: probe $PROBE_SERIAL, expecting $EXPECTED_DEVICE (${EXPECTED_LIFECYCLE:-any life cycle})"
# ENABLE_ACQUIRE 0 => no XRES / test-mode acquisition, so the running firmware is not reset
( cd "$OPENOCD_DIR" && "$OPENOCD" -s scripts -c "set ENABLE_ACQUIRE 0" \
    -f interface/kitprog3.cfg -c "adapter serial $PROBE_SERIAL" \
    -f "$OPENOCD_TARGET_CFG" -c "$OPENOCD_NO_PORTS" -c "init; targets; exit" ) >"$LOG" 2>&1
rc=$?

get() { grep -m1 "$1" "$LOG" | sed "s/.*$1[[:space:]]*:\{0,1\}[[:space:]]*//" | tr -d '\r'; }
dev="$(get 'Detected Device:')"
lcs="$(get 'Life Cycle  :')"
boot="$(get 'Boot Status :')"
ser="$(grep -m1 -o 'serial=[0-9A-Fa-f]*' "$LOG" | cut -d= -f2)"
kp="$(get 'KitProg3: FW version:')"
vt="$(get 'VTarget =')"
rev="$(grep -m1 -o 'Rev.: 0x[0-9A-F]* ([A-Z0-9]*)' "$LOG")"

printf '%-14s %s\n' "probe serial" "${ser:-?}" "KitProg3 FW" "${kp:-?}" "VTarget" "${vt:-?}" \
  "device" "${dev:-?}" "silicon" "${rev:-?}" "life cycle" "${lcs:-?}" "boot status" "${boot:-?}"
resolve_console; printf '%-14s %s\n' "console port" "${CONSOLE:-?}"

fail=0
[ "$ser" = "$PROBE_SERIAL" ] || { echo "FAIL: probe serial '$ser' != '$PROBE_SERIAL' (wrong/missing probe? run discover.sh)"; fail=1; }
if [ -z "$EXPECTED_DEVICE" ]; then
  echo "FAIL: EXPECTED_DEVICE is empty in $BOARD_DIR/board.env - detected '${dev:-nothing}'; confirm and put it there"; fail=1
elif [ "$dev" != "$EXPECTED_DEVICE" ]; then echo "FAIL: device '$dev' != '$EXPECTED_DEVICE'"; fail=1; fi
# Life cycle / boot status are usually NOT printed without test-mode acquire;
# flash.sh checks the life cycle with an acquire before programming.
if [ -z "$lcs" ]; then echo "NOTE: life cycle not reported by read-only attach (flash.sh checks it before programming)"
elif [ -n "$EXPECTED_LIFECYCLE" ] && [ "$lcs" != "$EXPECTED_LIFECYCLE" ]; then echo "FAIL: life cycle '$lcs' != '$EXPECTED_LIFECYCLE'"; fail=1; fi
[ $rc -eq 0 ] || { echo "FAIL: openocd exit $rc (probe busy in another tool? see $LOG)"; fail=1; }
[ -z "$boot" ] || [ "$boot" = "CYBOOT_SUCCESS" ] || echo "NOTE: boot status '$boot' (old image may be broken; not fatal)"
grep -q 'CMSIS-DAPv2' "$LOG" || echo "NOTE: probe not in CMSIS-DAPv2 bulk mode (fw-loader --mode kp3-bulk; ask the user first)"

if [ $fail -eq 0 ]; then echo "IDENTITY: PASS  (log $LOG)"; else echo "IDENTITY: FAIL  (log $LOG)"; fi
exit $fail

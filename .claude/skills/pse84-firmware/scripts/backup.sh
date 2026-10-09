#!/usr/bin/env bash
# Recovery prep: read (dump) the board's external-flash application area BEFORE the first
# flash, so the factory / previous firmware can be restored with flash.sh --hex.
# Usage: backup.sh <board-id> [<start> <size>] [--out <dir>]
#   start/size  default BACKUP_START / BACKUP_SIZE from the board profile,
#               else 0x60000000 / 0x00C00000 (12 MB: boot + S-M33 + NS-M33 + CM55 slots)
#   --out       default $PSE84_BACKUP_DIR = <workspace>/../backup (outside apps/, so clean.sh keeps it)
# Reads only, but the acquire RESETS the chip. Writes <out>/<board>-<probe>-<ts>.bin + .hex + .sha256
# Restore: flash.sh <any-app-of-this-board> --yes --hex <file>.hex   (ask the user first)

. "$(dirname "$0")/env.sh"
BOARD=""; START=""; SIZE=""; OUT=""
while [ $# -gt 0 ]; do
  case "$1" in --out) OUT="$2"; shift;; --*) die "unknown option $1";;
    *) if [ -z "$BOARD" ]; then BOARD="$1"; elif [ -z "$START" ]; then START="$1"; else SIZE="$1"; fi;; esac; shift
done
[ -n "$BOARD" ] || die "usage: backup.sh <board-id> [<start> <size>] [--out <dir>]"
load_board "$BOARD"
require_vars PROBE_SERIAL EXPECTED_DEVICE OPENOCD_TARGET_CFG
START="${START:-${BACKUP_START:-0x60000000}}"; SIZE="${SIZE:-${BACKUP_SIZE:-0x00C00000}}"
OUT="${OUT:-$PSE84_BACKUP_DIR}"; mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
require_safe_path "$OUT" "backup"
NAME="$BOARD_ID-$PROBE_SERIAL-$(ts)"; BIN="$OUT/$NAME.bin"; LOG="$OUT/$NAME.log"

bash "$SKILL_DIR/scripts/identity_check.sh" "$BOARD_ID" "$OUT/$NAME-identity.log" || die "identity gate failed"
# Any app of this board provides the SMIF flash loader; fall back to the openocd default.
# Read through the SMIF flash bank (runs the flash loader, which maps the QSPI part); a plain
# dump_image after acquire returns zeros because SMIF/XIP is not initialised in test mode.
[ $((START)) -ge $((0x60000000)) ] && [ $((START)) -lt $((0x68000000)) ] || die "start must be in SMIF 0x60000000..0x67FFFFFF"
OFFSET=$(printf '0x%X' $((START - 0x60000000)))
GEN="$(ls -d "$PSE84_WS"/*/bsps/*/config/GeneratedSource 2>/dev/null | head -1)"
info "Reading $SIZE bytes from $START (acquire resets the chip; takes a few minutes)"
( cd "$OPENOCD_DIR" && "$OPENOCD" -s scripts ${GEN:+-s "$(cygpath -m "$GEN")" -c "set QSPI_FLASHLOADER $(cygpath -m "$GEN")/PSE84_SMIF.FLM"} \
    -c "source [find interface/kitprog3.cfg]; adapter serial $PROBE_SERIAL; transport select swd; source [find $OPENOCD_TARGET_CFG]; adapter speed 12000" \
    -c "$OPENOCD_NO_PORTS" \
    -c "init; reset init; flash read_bank cat1d.cm33.smif1_ns {$(cygpath -m "$BIN")} $OFFSET $SIZE; reset run; shutdown" ) >"$LOG" 2>&1
rc=$?
[ $rc -eq 0 ] && [ -f "$BIN" ] || { tail -15 "$LOG"; die "dump failed (log $LOG)"; }
got=$(stat -c %s "$BIN"); [ "$got" -eq $((SIZE)) ] || die "dump size $got != $((SIZE))"
# sanity: all-0xFF or all-0x00 means nothing was read (SMIF not mapped)
"$PYTHON" - "$BIN" <<'EOF' || die "dump looks empty (all 0xFF/0x00) - SMIF not readable this way; do not rely on it"
import sys
d = open(sys.argv[1], "rb").read()
used = sum(1 for i in range(0, len(d), 4096) if d[i:i+4096].strip(b"\xff") and d[i:i+4096].strip(b"\x00"))
print(f"non-empty 4 KB pages: {used} of {len(d)//4096}")
sys.exit(0 if used else 1)
EOF
"$GCC_DIR/bin/arm-none-eabi-objcopy" -I binary -O ihex --change-addresses "$START" "$BIN" "$OUT/$NAME.hex" \
  || die "objcopy to hex failed"
( cd "$OUT" && sha256sum "$NAME.bin" "$NAME.hex" > "$NAME.sha256" )
info "Backup: $OUT/$NAME.{bin,hex}  (sha256 in $NAME.sha256)"
echo "BACKUP: PASS"

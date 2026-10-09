#!/usr/bin/env bash
# Stage 4: build all three projects + signed/combined HEX, then write an artifact manifest.
# Usage: build.sh <app-dir> [--clean] [--getlibs] [--allow-warnings]   (options in any order)
# Gate: board-scope gate PASS (board_gate.py app), make exit 0, fresh app_combined.hex, secure image
# signed, 0 warnings.
# Exit 0 = PASS, 1 = FAIL, 2 = built but with warnings (PASS only with --allow-warnings).
# build/manifest.txt is written only on PASS; flash.sh refuses to flash without it.

. "$(dirname "$0")/env.sh"
APPDIR=""; CLEAN=0; GETLIBS=0; ALLOWW=0
for a in "$@"; do
  case "$a" in
    --clean) CLEAN=1;; --getlibs) GETLIBS=1;; --allow-warnings) ALLOWW=1;;
    --*) die "unknown option $a";;
    *) APPDIR="$a";;
  esac
done
[ -d "$APPDIR" ] || die "usage: build.sh <app-dir> [--clean] [--getlibs] [--allow-warnings]"
APPDIR="$(cd "$APPDIR" && pwd)"
require_safe_path "$APPDIR" "app"
mkdir -p "$APPDIR/logs"; LOG="$APPDIR/logs/build-$(ts).log"
M="$APPDIR/build/manifest.txt"; HEX="$APPDIR/build/app_combined.hex"
rm -f "$M"                                   # no manifest unless this build passes
[ $CLEAN = 1 ] && { info "make clean"; mtb_make "$APPDIR" clean >>"$LOG" 2>&1 || die "make clean failed ($LOG)"; }
if [ $GETLIBS = 1 ] || [ ! -d "$APPDIR/../mtb_shared" ]; then
  info "make getlibs"; mtb_make "$APPDIR" getlibs >>"$LOG" 2>&1 || die "getlibs failed ($LOG)"
fi

# Board-scope gate: the app's own code (outside board/) must not use parts its board lacks
# (e.g. QWA309 base-board pins in an app made for the bare AI Kit). board_gate.py, components.json
gate="$("$PYTHON" "$SKILL_DIR/scripts/board_gate.py" app "$(winpath "$APPDIR")" 2>&1)"   || { printf '%s
' "$gate"; echo "BUILD: BLOCKED (board scope)"; exit 1; }
info "make build -j8  (log $LOG)"
marker="$(mktemp)"; t0=$SECONDS
mtb_make "$APPDIR" build -j8 >>"$LOG" 2>&1; rc=$?
dt=$((SECONDS-t0))
warn=$(grep -E ':[0-9]+:[0-9]+: warning:|ld(\.exe)?: warning:' "$LOG" | sort -u | wc -l)
errs=$(grep -E ':[0-9]+:[0-9]+: (fatal )?error:' "$LOG" | sort -u | wc -l)
signed=$(grep -c 'command "sign" succeeded' "$LOG")
fresh=0; [ -f "$HEX" ] && [ "$HEX" -nt "$marker" ] && fresh=1
# a no-op incremental build keeps the old (already signed) hex: accept it when make says up to date
[ $fresh = 0 ] && [ -f "$HEX" ] && [ $rc -eq 0 ] && grep -q 'no work to do\|Nothing to be done' "$LOG" && fresh=1 && signed=1
rm -f "$marker"

echo "make exit=$rc  time=${dt}s  warnings=$warn  errors=$errs  sign-ok=$signed  fresh-hex=$fresh"
if [ $rc -ne 0 ] || [ $fresh = 0 ] || [ "$signed" -eq 0 ]; then
  if grep -q 'BOARD GATE' "$LOG"; then    # compile-time board-scope gate (board/qwa309.h #error)
    grep -m1 -o 'BOARD GATE[^"]*' "$LOG"
    echo "BUILD: BLOCKED (board scope, compile time)  (log $LOG)"; exit 1
  fi
  grep -E 'error:|Error|\*\*\*' "$LOG" | head -20
  [ "$signed" -eq 0 ] && [ $rc -eq 0 ] && echo "secure image was not signed (Edge Protect Security Suite installed?)"
  echo "BUILD: FAIL  (log $LOG)"; exit 1
fi
if [ "$warn" -gt 0 ]; then
  grep -E ':[0-9]+:[0-9]+: warning:|ld(\.exe)?: warning:' "$LOG" | sort -u | head -20
  [ $ALLOWW = 1 ] || { echo "BUILD: WARNINGS ($warn) - fix them, or rerun with --allow-warnings"; exit 2; }
fi

# Artifact manifest: tool versions, libraries, sizes, hashes
{
  echo "# build manifest $(date '+%Y-%m-%d %H:%M:%S')  app=$(basename "$APPDIR")"
  [ -f "$APPDIR/pse84-app.env" ] && grep -v '^#' "$APPDIR/pse84-app.env"
  echo "MTB_TOOLS=$MTB_TOOLS_DIR"
  echo "GCC=$("$GCC_DIR/bin/arm-none-eabi-gcc" -dumpversion 2>/dev/null)"
  echo "EDGE_PROTECT=$(basename "$EDGEPROTECT_DIR" 2>/dev/null | sed 's/.*Suite-//')"
  echo "warnings=$warn sign_ok=$signed build_log=$(basename "$LOG")"
  echo "## libraries actually referenced by this app (deps/*.mtb, *.mtbx)"
  cat "$APPDIR"/bsps/*/deps/*.mtbx "$APPDIR"/proj_*/deps/*.mtb 2>/dev/null \
    | sed -n 's|^[^#]*/\([^/#]*\)#\([^#]*\)#.*|\1 \2|p' | sed 's/\.git / /' | sort -u
  echo "## artifacts: bytes sha256 path"
  for f in "$HEX" "$APPDIR"/build/project_hex/*.hex "$APPDIR"/build/project_hex/*.elf; do
    [ -f "$f" ] && echo "$(stat -c %s "$f") $(sha256sum "$f" | cut -d' ' -f1) ${f#$APPDIR/}"
  done
} >"$M"
echo "manifest: $M"
grep app_combined "$M"
echo "BUILD: PASS$([ "$warn" -gt 0 ] && echo " (with $warn allowed warning(s))")"
exit 0

#!/usr/bin/env bash
# Stage 2d: hand an app over to the IDE - VS Code + Infineon ModusToolbox for VS Code - so the
# developer can edit, build, program and debug it with the extension's own tasks and launch
# configurations. Runs ModusToolbox's own `make vscode` (once, or again with --refresh), which
# writes .vscode/ and <app>.code-workspace for all projects of the app (proj_cm33_s,
# proj_cm33_ns, proj_cm55), applies the extension's own task/settings fix (vscode_fix.py), then
# opens that workspace - never the app folder itself.
# new_app.sh runs this after creating the app (--no-open there: no window).
# Usage: open_ide.sh <app-dir> [--no-open] [--refresh]
#   --refresh   re-run make vscode (after a BSP/library change, another PC or another ModusToolbox)
# Gate: "IDE: READY <workspace>" (+ "IDE: OPENED"). Exit 10 ACTION: SETUP = VS Code or the
# extension is missing (installing them is the developer's).
#
# The IDE works on the same project and outputs as the scripts: the same Makefiles, mtb_shared
# and build/ folders as build.sh; "Build"/"Program" tasks = make build / make program (KitProg3 +
# OpenOCD, without the skill's identity gate). The generated files hold this PC's tool paths:
# they are git-ignored and re-made per PC.

. "$(dirname "$0")/env.sh"
APPDIR=""; OPEN=1; REFRESH=0
for a in "$@"; do
  case "$a" in --no-open) OPEN=0;; --refresh) REFRESH=1;; --*) die "unknown option $a";; *) APPDIR="$a";; esac
done
[ -n "$APPDIR" ] && [ -f "$APPDIR/pse84-app.env" ] || die "usage: open_ide.sh <app-dir> [--no-open] [--refresh]  (an app made by new_app.sh)"
APPDIR="$(cd "$APPDIR" && pwd)"; APP="$(basename "$APPDIR")"
WSF="$APPDIR/$APP.code-workspace"
if [ $REFRESH = 1 ] || [ ! -f "$WSF" ] || [ ! -d "$APPDIR/.vscode" ]; then
  mkdir -p "$APPDIR/logs"; LOG="$APPDIR/logs/make-vscode-$(ts).log"
  info "make vscode (ModusToolbox: .vscode/ + $APP.code-workspace; log $LOG)"
  mtb_make "$APPDIR" vscode >"$LOG" 2>&1 || { tail -15 "$LOG"; die "make vscode failed ($LOG) - libraries missing? run build.sh --getlibs"; }
  [ -f "$WSF" ] || die "make vscode did not write $WSF ($LOG)"
fi
# the extension's "Fix Tasks" / "Fix Settings" applied up front (tools 3.9 make vscode writes an
# older format than extension 1.12 expects); idempotent, values come from the make vscode output
"$PYTHON" "$SKILL_DIR/scripts/vscode_fix.py" "$(winpath "$APPDIR")" || die "vscode_fix.py failed"
fw_open_ide "$WSF" $OPEN
cat <<'TXT'
  In VS Code (Infineon ModusToolbox for VS Code, ModusToolbox Assistant view):
    first open    the extension loads the app (tasks/settings already in its 1.12 format - if a newer
                  extension still offers "Fix Tasks" / "Fix Settings", accept it)
    build         Terminal > Run Build Task "Build" - same Makefiles and build/ as build.sh
    flash         Run Task "Program" / "Build & Program" (KitProg3 + OpenOCD; no identity gate -
                  with several probes set MTB_PROBE_SERIAL in bsp.mk and re-run open_ide.sh --refresh)
    debug         Run and Debug: "Multi-Core Debug", "Launch/Attach PSOCE84 CM33/CM55 (KitProg3_MiniProg4)"
    test          serial_test.py with tests/*.json (stage 6), or any serial terminal on the KitProg3 COM port
    hardware      Device Configurator from the Assistant view; then rebuild (generated sources)
TXT

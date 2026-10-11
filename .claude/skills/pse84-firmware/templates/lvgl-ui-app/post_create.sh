#!/usr/bin/env bash
# Template hook (new_app.sh): generate the UI code from the starter spec ui/ui.json.
# Env: PYTHON, SKILLS_DIR (the .claude/skills folder). Arg: the new app's folder.
set -e
APPDIR="$1"
"$PYTHON" "$SKILLS_DIR/ui-designer/scripts/ui_gen.py" "$APPDIR/ui/ui.json" "$APPDIR"

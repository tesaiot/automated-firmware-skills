#!/usr/bin/env bash
# Stage 2b: create a buildable ModusToolbox application for a board profile.
# No board needed - the identity gate is only required before flashing.
# Usage: new_app.sh <board-id> <app-name> [workspace-dir] [template]
#        new_app.sh <board-id> <app-name> [workspace-dir] --example <id> [--example-commit <tag>]
#   app-name       letters, digits, - and _ only
#   workspace-dir  where apps + mtb_shared live (default: $PSE84_WS, "" = default)
#   template       name under templates/ (default: hello-world), or "none" = keep vendor code
#   --example      start from an Infineon code example instead (list: examples.sh <board>);
#                  pinned to its newest release for this BSP unless --example-commit is given.
#                  The example's own code is kept (no layered template on top).
#
# 1. get the app (pinned hello-world template, or the example via Project Creator)
# 2. Project Creator: app + base BSP               5. apply overlays (design.modus, extras)
# 3. optional: replace the BSP from a pinned git   6. regenerate GeneratedSource (configurator CLIs)
#    source (BSP_SOURCE=git:...), then getlibs     7. copy layered code template, set BOARD define
# 4. gate: BSP version + base design.modus hash
# Writes <app>/pse84-app.env (board, template, versions, hashes) and logs into <app>/logs/.

. "$(dirname "$0")/env.sh"
POS=(); EXAMPLE=""; EXCOMMIT=""
while [ $# -gt 0 ]; do
  case "$1" in
    --example) EXAMPLE="$2"; shift;; --example-commit) EXCOMMIT="$2"; shift;;
    --*) die "unknown option $1";;
    *) POS+=("$1");;
  esac; shift
done
[ -n "${POS[1]}" ] || die "usage: new_app.sh <board-id> <app-name> [workspace-dir] [template | --example <id>]"
load_board "${POS[0]}"; APP="${POS[1]}"; WS="${POS[2]:-$PSE84_WS}"; TPL="${POS[3]:-hello-world}"
[ -n "$EXAMPLE" ] && TPL=none
case "$APP" in *[!A-Za-z0-9_-]*|"") die "app name '$APP': use only letters, digits, - and _";; esac
require_vars BSP_ID APP_TEMPLATE_REPO APP_TEMPLATE_COMMIT BOARD_DEFINE
mkdir -p "$WS" || die "cannot create $WS"; WS="$(cd "$WS" && pwd)"; APPDIR="$WS/$APP"
require_safe_path "$APPDIR" "app"
[ -n "$(path_synced "$WS")" ] && echo "WARN: $WS is in a $(path_synced "$WS") folder - pause sync if getlibs/build fail on locked files"
[ -e "$APPDIR" ] && die "$APPDIR already exists (pick another name, or clean.sh it first)"
[ "$TPL" = none ] || { [ -d "$SKILL_DIR/templates/$TPL" ] && [ "${TPL#_}" = "$TPL" ]; } \
  || die "no template '$TPL' (have: $(ls "$SKILL_DIR/templates" | grep -v '^_' | tr '\n' ' '))"
# Board-scope gate: a template may list the parts it needs in requires.txt (ids from
# boards/components.json); the board must have them (e.g. pots need board id tesaiot)
if [ "$TPL" != none ] && [ -f "$SKILL_DIR/templates/$TPL/requires.txt" ]; then
  "$PYTHON" "$SKILL_DIR/scripts/board_gate.py" check "$BOARD_ID" $(grep -v '^#' "$SKILL_DIR/templates/$TPL/requires.txt")     || die "template '$TPL' needs parts board '$BOARD_ID' does not have (GATE: BLOCKED above)"
fi
mkdir -p "$WS/.cache"
TMPLOG="$WS/.cache/project-creator-$APP.log"

if [ -n "$EXAMPLE" ]; then
  # 1+2. Infineon code example: resolve uri + pinned release from the manifest, clone via Project Creator
  case "$EXAMPLE" in mtb-*|avnet-*|kit-*|kit_*) ;; *) EXAMPLE="mtb-example-psoc-edge-$EXAMPLE";; esac
  MTBV="$(basename "$MTB_TOOLS_DIR" | sed 's/tools_//')"
  read -r EXURI EXPIN < <("$PYTHON" "$SKILL_DIR/scripts/examples.py" --bsp "$BSP_ID" --tools "${MTBV:-3.9}" --resolve "$EXAMPLE" | tr -d '\r') \
    || die "example '$EXAMPLE' is not available for $BSP_ID (see examples.sh $BOARD_ID)"
  [ -n "$EXURI" ] || die "example '$EXAMPLE' is not available for $BSP_ID (see examples.sh $BOARD_ID)"
  EXCOMMIT="${EXCOMMIT:-$EXPIN}"
  APP_SRC="$EXURI@$EXCOMMIT"
  # Project Creator needs URI+commit for the BSP too when the app has them -> the BSP is pinned as well
  [ -n "$BSP_VERSION" ] || die "BSP_VERSION is empty in the profile (needed to pin the BSP for an example)"
  BSP_URI="${BSP_URI:-https://github.com/Infineon/TARGET_${BSP_ID}}"
  BSP_COMMIT="${BSP_COMMIT:-release-v${BSP_VERSION}}"
  info "Project Creator: example $EXAMPLE @ $EXCOMMIT, BSP $BSP_ID @ $BSP_COMMIT -> $APPDIR  (takes a few minutes)"
  "$PROJECT_CREATOR" -b "$BSP_ID" --board-uri "$BSP_URI" --board-commit "$BSP_COMMIT" \
    -a "$EXAMPLE" --app-uri "$EXURI" --app-commit "$EXCOMMIT" \
    -d "$(cygpath -w "$WS")" --user-app-name "$APP" --use-modus-shell >"$TMPLOG" 2>&1 \
    || { tail -20 "$TMPLOG"; die "project-creator failed (log $TMPLOG)"; }
else
  # 1. pinned template app, cached per commit
  CACHE="$WS/.cache/$(basename "$APP_TEMPLATE_REPO" .git)@${APP_TEMPLATE_COMMIT:0:12}"
  if [ ! -d "$CACHE/.git" ]; then
    info "Cloning $APP_TEMPLATE_REPO @ $APP_TEMPLATE_COMMIT"
    git clone -q "$APP_TEMPLATE_REPO" "$CACHE" && git -C "$CACHE" checkout -q "$APP_TEMPLATE_COMMIT" \
      || die "clone/checkout failed (network/proxy? GitHub must be reachable)"
  fi
  [ "$(git -C "$CACHE" rev-parse HEAD)" = "$APP_TEMPLATE_COMMIT" ] || die "template cache $CACHE is not at $APP_TEMPLATE_COMMIT"
  APP_SRC="$APP_TEMPLATE_REPO@$APP_TEMPLATE_COMMIT"
  # 2. Project Creator (imports the app, clones the BSP, runs getlibs)
  info "Project Creator: board $BSP_ID -> $APPDIR  (takes a few minutes)"
  "$PROJECT_CREATOR" -b "$BSP_ID" --app-path "$(cygpath -w "$CACHE")" -d "$(cygpath -w "$WS")" \
    --user-app-name "$APP" --use-modus-shell >"$TMPLOG" 2>&1 || { tail -20 "$TMPLOG"; die "project-creator failed (log $TMPLOG)"; }
fi
mkdir -p "$APPDIR/logs"; mv "$TMPLOG" "$APPDIR/logs/project-creator.log"

# BSP folder: detect instead of trusting the profile
bsps=("$APPDIR"/bsps/*/); [ ${#bsps[@]} -eq 1 ] && [ -d "${bsps[0]}" ] || die "expected exactly one BSP under $APPDIR/bsps"
DET_DIR="$(basename "${bsps[0]}")"
[ -n "$BSP_DIR" ] && [ "$BSP_DIR" != "$DET_DIR" ] && echo "WARN: BSP folder is $DET_DIR, profile says $BSP_DIR - update board.env"
BSP_DIR="$DET_DIR"; BSP="$APPDIR/bsps/$BSP_DIR"

# 3. optional BSP replacement from a pinned git source: BSP_SOURCE=git:<repo-url>@<commit>:<subdir>
# Use when the board vendor ships a modified BSP (e.g. TESAIoT's TARGET_KIT_PSE84_AI).
# Prefer porting deltas onto the current vendor BSP instead (reference/project.md): an old BSP
# copied wholesale drags old library versions with it.
case "$BSP_SOURCE" in
  git:*)
    spec="${BSP_SOURCE#git:}"; sub="${spec##*:}"; spec="${spec%:*}"; repo="${spec%@*}"; commit="${spec##*@}"
    [ -n "$repo" ] && [ -n "$commit" ] && [ -n "$sub" ] || die "BSP_SOURCE must be git:<url>@<commit>:<subdir>"
    BCACHE="$WS/.cache/bsp-$(basename "$repo" .git)@${commit:0:12}"
    if [ ! -d "$BCACHE/.git" ]; then
      info "Cloning BSP source $repo @ $commit"
      git clone -q "$repo" "$BCACHE" && git -C "$BCACHE" checkout -q "$commit" || die "BSP source clone failed"
    fi
    [ -d "$BCACHE/$sub" ] || die "no $sub in $repo@$commit"
    info "Replacing $BSP_DIR with $sub from $repo@${commit:0:12}"
    mv "$BSP" "$APPDIR/logs/bsp-vendor-$BSP_DIR"
    cp -r "$BCACHE/$sub" "$BSP"
    info "make getlibs (BSP dependencies changed)"
    mtb_make "$APPDIR" getlibs >"$APPDIR/logs/getlibs-bsp-source.log" 2>&1 || die "getlibs failed (logs/getlibs-bsp-source.log)"
    ;;
  ""|vendor) ;;
  *) die "unknown BSP_SOURCE '$BSP_SOURCE'";;
esac

# 4. gate: BSP version and the exact base design.modus the overlay was made from
bspver="$(sed -n 's/.*"version": *"\([0-9.]*\)".*/\1/p' "$BSP/props.json" 2>/dev/null | head -1)"
info "BSP $BSP_DIR version ${bspver:-?} (profile expects ${BSP_VERSION:-unset})"
[ -z "$BSP_VERSION" ] || case "$bspver" in "$BSP_VERSION"*) ;; *) echo "WARN: BSP version differs from profile";; esac
base="$(sha256sum "$BSP/config/design.modus" | cut -d' ' -f1)"
info "base design.modus sha256 $base"
if [ -n "$BSP_BASE_MODUS_SHA256" ] && [ "$base" != "$BSP_BASE_MODUS_SHA256" ]; then
  [ -n "$BSP_OVERLAY_MODUS" ] && die "base design.modus changed upstream ($base != $BSP_BASE_MODUS_SHA256).
     The overlay is only valid on the old base - re-port it (reference/project.md 'Porting a BSP delta')."
  echo "WARN: base design.modus differs from profile - review GeneratedSource before flashing"
fi

# 5. overlays (before regeneration)
cp "$BSP/config/design.modus" "$APPDIR/logs/design.modus.base"
regen_qspi=0
if [ -n "$BSP_OVERLAY_MODUS" ]; then
  f="$BOARD_DIR/$BSP_OVERLAY_MODUS"; [ -f "$f" ] || f="$BOARD_PARENT_DIR/$BSP_OVERLAY_MODUS"
  [ -f "$f" ] || die "overlay $BSP_OVERLAY_MODUS missing in $BOARD_DIR${BOARD_PARENT_DIR:+ and $BOARD_PARENT_DIR}"
  if [ -n "$BSP_OVERLAY_MODUS_SHA256" ] && [ "$(sha256sum "$f" | cut -d' ' -f1)" != "$BSP_OVERLAY_MODUS_SHA256" ]; then
    die "overlay $f does not match BSP_OVERLAY_MODUS_SHA256 (edited without updating board.env?)"
  fi
  info "Overlay $BSP_OVERLAY_MODUS -> config/design.modus"
  cp "$f" "$BSP/config/design.modus"
fi
for pair in $BSP_OVERLAY_EXTRA; do          # "src:dst" pairs, src under board dir, dst under BSP
  src="$BOARD_DIR/${pair%%:*}"; dst="$BSP/${pair##*:}"
  [ -f "$src" ] || die "overlay extra $src missing"
  info "Overlay ${pair%%:*} -> ${pair##*:}"
  mkdir -p "$(dirname "$dst")"; cp "$src" "$dst"
  case "$dst" in *.cyqspi) regen_qspi=1;; esac
done

# 6. regenerate configurator output (make build does NOT do this)
if [ $regen_qspi = 1 ] && [ -x "$QSPI_CONFIGURATOR" ]; then
  info "Regenerating QSPI configuration"
  ( cd "$BSP/config" && "$QSPI_CONFIGURATOR" --build "$(cygpath -w "$BSP/config/design.cyqspi")" ) \
    >"$APPDIR/logs/qspi-configurator.log" 2>&1 || die "qspi-configurator-cli failed (logs/qspi-configurator.log)"
fi
if [ -n "$BSP_OVERLAY_MODUS$BSP_OVERLAY_EXTRA" ] || [ "${BSP_SOURCE:-vendor}" != vendor ]; then
  info "Regenerating GeneratedSource (device-configurator-cli, no --library)"
  ( cd "$BSP/config" && "$DEVICE_CONFIGURATOR" --build "$(cygpath -w "$BSP/config/design.modus")" ) \
    >"$APPDIR/logs/device-configurator.log" 2>&1 || die "device-configurator-cli failed (logs/device-configurator.log)"
fi

# 7. layered source template + board define
if [ "$TPL" != none ]; then
  info "Applying template $TPL"
  # templates/_common (board/ + func/ layers) first, then the app template on top
  for T in "$SKILL_DIR/templates/_common" "$SKILL_DIR/templates/$TPL"; do
    for proj in proj_cm33_s proj_cm33_ns proj_cm55; do
      [ -d "$T/$proj" ] && cp -r "$T/$proj/." "$APPDIR/$proj/"
    done
  done
  T="$SKILL_DIR/templates/$TPL"
  [ -d "$T/tests" ] && cp -r "$T/tests" "$APPDIR/"
  [ -f "$T/README.md" ] && cp "$T/README.md" "$APPDIR/README-app.md"
  # shared/ = headers common to both cores (e.g. the CM33<->CM55 mailbox contract)
  if [ -d "$T/shared" ]; then
    mkdir -p "$APPDIR/shared" && cp -r "$T/shared/." "$APPDIR/shared/"
    for proj in proj_cm33_ns proj_cm55; do
      mk="$APPDIR/$proj/Makefile"
      if grep -q '^INCLUDES+=' "$mk"; then
        sed -i "0,/^INCLUDES+=/s//INCLUDES+=..\/shared /" "$mk"
      else
        echo "INCLUDES+=../shared" >>"$mk"
      fi
      grep -q '^INCLUDES+=.*\.\./shared' "$mk" || die "could not add ../shared to $mk"
    done
  fi
fi
for proj in proj_cm33_ns proj_cm55; do
  mk="$APPDIR/$proj/Makefile"; [ -f "$mk" ] || continue   # examples may have other projects
  if grep -q '^DEFINES+=' "$mk"; then
    sed -i "0,/^DEFINES+=/s//DEFINES+=$BOARD_DEFINE /" "$mk"
  else
    echo "DEFINES+=$BOARD_DEFINE" >>"$mk"
  fi
  grep -q "DEFINES+=.*\b$BOARD_DEFINE\b" "$mk" || die "could not add $BOARD_DEFINE to $mk"
done

cat >"$APPDIR/pse84-app.env" <<EOF
# written by pse84-firmware/new_app.sh $(date '+%Y-%m-%d %H:%M')
APP_BOARD=$BOARD_ID
APP_BOARD_DEFINE=$BOARD_DEFINE
APP_TEMPLATE=$TPL
APP_EXAMPLE=${EXAMPLE:-none}
APP_BSP=$BSP_ID ${bspver:-?} ($BSP_DIR)
APP_BSP_SOURCE=${BSP_SOURCE:-vendor}
APP_BSP_BASE_MODUS_SHA256=$base
APP_BSP_OVERLAY=${BSP_OVERLAY_MODUS:-none} ${BSP_OVERLAY_MODUS_SHA256}
APP_TEMPLATE_SRC=$APP_SRC
APP_MTB_TOOLS=$(basename "$MTB_TOOLS_DIR")
EOF
info "Created $APPDIR  (board $BOARD_ID, template $TPL)"
# Stage 2d: prepare the VS Code workspace (no window). The app exists either way, so a missing
# IDE/extension is reported as a note and new_app.sh still succeeds.
ide_out="$(bash "$SKILL_DIR/scripts/open_ide.sh" "$APPDIR" --no-open 2>&1)"; ide_rc=$?
printf '%s
' "$ide_out" | grep -E '^(IDE:|ACTION:|ERROR:)' | sed 's/^ACTION: SETUP /IDE: NOT READY - developer action: /'
[ $ide_rc = 0 ] || [ $ide_rc = 10 ] || echo "WARN: open_ide.sh failed (exit $ide_rc) - the app itself was created"
info "Next: build.sh $APPDIR   (or open_ide.sh $APPDIR to work in VS Code)"

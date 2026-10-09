#!/usr/bin/env bash
# List / describe the ModusToolbox code examples (Infineon manifest) that support a board's BSP.
# Usage: examples.sh <board-id> [words...]          list, filtered by words (all must match)
#        examples.sh <board-id> --category <text>    one category (e.g. Bluetooth, Wi-Fi, Graphics)
#        examples.sh <board-id> --detail <id>        name, description, releases, README link
#        examples.sh <board-id> --refresh ...        re-download the manifest (cached 1 day)
# Ids may be written without the "mtb-example-psoc-edge-" prefix.
# Note: examples target the vendor kit (KIT_PSE84_AI for kit-pse84-ai and tesaiot). On a board with a BSP overlay the clocks are
# ported, but check the example README's hardware needs against boards/<id>/README.md first.

. "$(dirname "$0")/env.sh"
[ -n "$1" ] || die "usage: examples.sh <board-id> [words... | --detail <id> | --category <text>]"
load_board "$1"; shift
require_vars BSP_ID
ARGS=()
while [ $# -gt 0 ]; do
  case "$1" in
    --detail|--resolve) id="$2"; case "$id" in mtb-*|*-mtb-*|avnet-*|kit-*|kit_*) ;; *) id="mtb-example-psoc-edge-$id";; esac
                        ARGS+=("$1" "$id"); shift;;
    --category) ARGS+=("$1" "$2"); shift;;
    *) ARGS+=("$1");;
  esac; shift
done
MTBV="$(basename "$MTB_TOOLS_DIR" | sed 's/tools_//')"
[ $# -eq 0 ] || true
echo "Board: $BOARD_ID ($BOARD_NAME) -> BSP $BSP_ID"
"$PYTHON" "$SKILL_DIR/scripts/examples.py" --bsp "$BSP_ID" --tools "${MTBV:-3.9}" "${ARGS[@]}"

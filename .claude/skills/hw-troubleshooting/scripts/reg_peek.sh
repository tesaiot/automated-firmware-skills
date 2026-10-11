#!/usr/bin/env bash
# READ-ONLY register peek on the RUNNING firmware of a PSOC Edge E84 board: OpenOCD attach
# without acquire (ENABLE_ACQUIRE 0) - no reset, halt, erase, program or register write.
# Use it to see how a firmware you cannot read (a vendor/SDK image, a known-good board) has
# configured pins and peripherals, and diff it against ours.
#
# Usage: reg_peek.sh <board-id> [--serial <probe-serial>] <profile|0xADDR:N[:label]> ...
#   profiles: scb5  = P16/P17 GPIO + HSIOM, SCB5 CTRL/STATUS/I2C regs, SCB5 clock divider
#             gpio<N> = GPIO port N (OUT..IN, CFG, CFG_IN, CFG_OUT) + its HSIOM SEL0/SEL1
#   --serial  another KitProg3 than the bench one (second board; fw-loader --device-list).
#             Never write the serial into a committed file.
# Addresses are the NON-SECURE aliases (0x4...): the vendor BSP assigns GPIO/SCB/PERI to the
# non-secure side and the secure aliases read back 0.
#
# Decoding (PSE84, PDL cyip_gpio.h / cyip_hsiom.h / cyip_scb.h / cyip_peri_pclk_v2_1.h):
#   GPIO_PRTn base 0x42810000 + 0x80*n: OUT +0x00, IN +0x10, CFG +0x44 (4 bits/pin:
#     bit3 = input buffer, bits2..0 = drive mode: 0 analog/high-Z, 2 pull-up, 4 open-drain-low,
#     6 strong), CFG_IN +0x48, CFG_OUT +0x4C
#   HSIOM_PRTn 0x42800000 + 0x10*n: SEL0 = pins 0-3, SEL1 = pins 4-7, 8 bits/pin (0 = GPIO)
#   SCB5 0x429D0000: CTRL +0x00, I2C_CTRL +0x60 (bits 3..0 high-phase OVS, 7..4 low-phase OVS,
#     each +1 = clocks), I2C_STATUS +0x64
#   PERI0 group 1 clocks 0x42042000: CLOCK_CTL[i] +0xC00 (+4*i; bits 9..8 divider type
#     0 = 8-bit, 1 = 16-bit; bits 7..0 divider #), DIV_8_CTL +0x1000, DIV_16_CTL +0x1400
#     (bits 15..8 / 23..8 = integer divide - 1). SCB5 clock = CLOCK_CTL[4].
#   I2C rate = clk_peri(100 MHz) / (divider+1) / (lowOVS+1 + highOVS+1)

. "$(dirname "$0")/../../pse84-firmware/scripts/env.sh"
BOARD="$1"; shift || true
[ -n "$BOARD" ] || die "usage: reg_peek.sh <board-id> [--serial S] <profile|0xADDR:N[:label]> ..."
load_board "$BOARD"
[ "$1" = "--serial" ] && { PROBE_SERIAL="$2"; shift 2; }
require_vars PROBE_SERIAL OPENOCD_TARGET_CFG
[ $# -gt 0 ] || set -- scb5

CMDS="init;"
add() { CMDS="$CMDS echo {== $3}; mdw $1 $2;"; }       # braces: Tcl must not evaluate [..]
for spec in "$@"; do
  case "$spec" in
    scb5)
      add 0x42810800 5 "GPIO P16 OUT OUT_CLR OUT_SET OUT_INV IN"; add 0x42810844 3 "GPIO P16 CFG CFG_IN CFG_OUT"
      add 0x42800100 2 "HSIOM P16 SEL0 SEL1"
      add 0x42810880 5 "GPIO P17 OUT OUT_CLR OUT_SET OUT_INV IN"; add 0x428108C4 3 "GPIO P17 CFG CFG_IN CFG_OUT"
      add 0x42800110 2 "HSIOM P17 SEL0 SEL1"
      add 0x429D0000 2 "SCB5 CTRL STATUS"; add 0x429D0060 3 "SCB5 I2C_CTRL I2C_STATUS I2C_M_CMD"
      add 0x42042C10 1 "SCB5 clock CLOCK_CTL 4"; add 0x42043000 4 "PERI0 grp1 DIV_8_CTL 0-3"
      add 0x42043400 4 "PERI0 grp1 DIV_16_CTL 0-3" ;;
    gpio[0-9]*)
      n="${spec#gpio}"; g=$(printf '0x%X' $((0x42810000 + 0x80 * n))); h=$(printf '0x%X' $((0x42800000 + 0x10 * n)))
      add "$g" 5 "GPIO P$n OUT OUT_CLR OUT_SET OUT_INV IN"; add "$(printf '0x%X' $((g + 0x44)))" 3 "GPIO P$n CFG CFG_IN CFG_OUT"
      add "$h" 2 "HSIOM P$n SEL0 SEL1" ;;
    0x*:*)
      IFS=: read -r a c l <<<"$spec"; add "$a" "$c" "${l:-$a}" ;;
    *) die "unknown profile/spec '$spec'" ;;
  esac
done
CMDS="$CMDS exit"

LOG="${TMPDIR:-/tmp}/reg_peek-$BOARD-$(ts).log"
( cd "$OPENOCD_DIR" && "$OPENOCD" -s scripts -c "set ENABLE_ACQUIRE 0" \
    -f interface/kitprog3.cfg -c "adapter serial $PROBE_SERIAL" \
    -f "$OPENOCD_TARGET_CFG" -c "$OPENOCD_NO_PORTS" -c "$CMDS" ) >"$LOG" 2>&1
rc=$?
grep -E "^==|^0x4|Error" "$LOG"
[ $rc -eq 0 ] && echo "PEEK: PASS (read-only, log $LOG)" || echo "PEEK: FAIL rc=$rc (log $LOG; probe busy in another tool?)"
exit $rc

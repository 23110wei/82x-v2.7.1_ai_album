#ifndef AI_ALBUM_POWER_CTRL_H
#define AI_ALBUM_POWER_CTRL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "typesdef.h"

/*
 * Board power latch (TXW827-RGB888 V2.0 schematic):
 *   PA5 -> R65 1K -> Q3(S8050) base; Q3 pulls SYS_EN low to keep the Q2
 *   P-MOS closed. R67 10K pulls the base low, so power is only held while
 *   PA5 is driven high. Power-on is hardware: SW8 pulls SYS_EN low through
 *   D5 until firmware latches PA5.
 */

/* Boot-time power latch. Key PA15 first, VBUS PA10 as fallback: no key +
 * VBUS high = external-power boot (USB plug / debug bench) -> latch at
 * once; key held -> must stay held through the minimum hold window (short
 * taps don't boot); otherwise enter a 200ms-recheck loop: latch and boot
 * the moment VBUS appears (plugging USB rescues the board - a flaky PA10
 * read can never brick it), while a battery brown-out ghost boot keeps
 * reading low until the rail dies. NOTE: PA_10 has NO ADC channel on this
 * chip (hgadc_v1 IO table skips PA9/PA10), so VBUS is digital-GPIO only
 * and marginal near the logic threshold. Blocks while undecided, so call
 * it first thing in the demo init. */
void power_ctrl_hold(void);

/* 1 when this boot was initiated by holding the power key (latch path).
 * The key bridge uses it to swallow the boot-hold press so keyWork's
 * long-press detection does not pop the power dialog right after boot;
 * the first power-key release re-arms normal handling. */
uint8_t power_ctrl_booted_with_key(void);

/* Drop the latch; the board loses power within milliseconds and this
 * never returns. Only call after the graceful shutdown sequence. */
void power_ctrl_release(void);

/* Graceful shutdown: stop audio session, flush/unmount SD FatFS, power the
 * panel down in reverse order, then cut system power. Never returns. */
void power_ctrl_shutdown_sequence(void);

/* External-power presence on PA10_USB_DET via the R56/R58 mega-ohm VBUS
 * divider, read as digital GPIO (PA_10 has no ADC channel on this chip).
 * The high-impedance source sits near the logic threshold, so readings
 * can flap - debounced over a few polls from one context (home runtime
 * timer). The JTAG TCK line shares the pin: readings are meaningless with
 * a debug probe attached. TP4056 CHRG#/STDBY# are not wired to the SoC,
 * so this is VBUS presence only: "charging" vs "charge complete" cannot
 * be distinguished - treat it as external power. */
uint8_t power_ctrl_charging(void);

#ifdef __cplusplus
}
#endif

#endif

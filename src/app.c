// ATU-100 NG firmware: main program.
//
// One main loop, no blocking waits except while tuning: it measures, keeps
// the peak power for the display, updates the display a little at a time,
// handles the button and the external interface, and runs the timers.

#include "board.h"
#include "icom_interface.h"
#include "timer.h"
#include "cells.h"
#include "meas.h"
#include "relays.h"
#include "tune.h"
#include "nvm.h"
#include "buttons.h"
#include "display.h"
#include "settings.h"
#include "setup.h"
#include "text.h"
#include "oled.h"
#include "uart.h"
#include "i2c_soft.h"
#include "version.h"

#define WATCH_MS    50         // measurement for the display / auto tune
#define SHOW_MS     150        // display update of power and SWR
#define REFRESH_MS  30000      // send the whole picture again
#define AUTO_STEADY 4          // auto tune after this many measurements in a row
#define AUTO_HOLD   3000       // ms after a tune without auto tune
#define RESUME_MS   60000      // a tune within this time continues an interrupted one
#define SWR_SHOW_UW FOLD_MIN_UW // SWR shown from 0.1 W (the lowest Cell 4), also below Cell 4
#define HINT_MS     1000       // a tune without a suitable carrier: the display says why
#define CARRIER_MS  (WAIT_START * 10UL)   // ... and gives up, as tune_run would
#define CARRIER_ON_MS 200      // a carrier that comes while waiting: this long, then tune
#define SETUP_HOLD  100        // x 10 ms held at the end of the greeting: setup menu

static uint16_t t_watch, t_show, t_refresh, t_active, t_led, t_msg, t_tuned;
static uint8_t msg_on, auto_cnt, auto_tune, low_cnt;
enum { B_OK, B_WARN, B_LOW, B_OFF };
static uint8_t row_lvl;        // the mildest level of the readings in the row so far
static uint16_t swr_ref;       // SWR of the last tune, reference for auto tune
static uint16_t shown_pwr = 0xFFFF, shown_swr = 0xFFFF;
static meas_t peak;            // peak hold for the display
static uint32_t t_peak;
static uint16_t swr_last;      // last SWR measured with enough power
static uint16_t swr_disp;      // last SWR measured from SWR_SHOW_UW on, for the display
static uint16_t swr_live;      // current SWR, zero when forward power is below display threshold
static uint16_t pwr_live;      // current power in 0.1 W, zero below display threshold
static uint8_t resume;         // the last tune can be continued: tune_resumable

// ms since the tick value t. Always read the clock fresh: a timestamp set
// in between tune may be later than a value read before, and the
// unsigned difference would then be huge.
static uint16_t since(uint16_t t) {
   return tick_ms() - t;
}

// ---------------------------------------------------------------- text

static char buf[8];

static const char *num(uint16_t v, uint8_t dec, uint8_t width) {
   fmt_num(buf, v, dec, width);
   return buf;
}

// ---------------------------------------------------------------- screen

// The label clears everything left of the value (x 0..59): whatever was
// there before (TUNE, a message) must not leave pixels in the gaps
static void show_swr_label(void) {
   disp_med(LINE2, 0, "       ");
   disp_med(LINE2, 0, st.bypass ? "BYP" : "SWR");
   disp_med(LINE2, 42, "=");
}

static const uint16_t l_10nh[7] = {5, 10, 22, 45, 100, 220, 450};
static const uint16_t c_pf[7] = {10, 22, 47, 100, 220, 470, 1000};
static uint16_t shown_rel = 0xFFFF;           // l | c << 7 | sw << 14

static void show_relays(const relays_t *r) {
   uint16_t key = (uint16_t)(r->l | (uint16_t)r->c << 7 | (uint16_t)r->sw << 14), l = 0, c = 0;
   uint8_t i;
   char *e; 
   if(key == shown_rel) return;
   shown_rel = key;
   for(i = 0; i < 7; i++) {
      if(r->l & 1 << i) l += l_10nh[i];
      if(r->c & 1 << i) c += c_pf[i];
   }   

   disp_cells(0, 10, r->l);   // Page 0 (y = 0..6, yellow)
   disp_cells(8, 10, r->c);   // Page 1 (y = 9..15, yellow)

   // Inductance value
   e = l < 1000 ? fmt_num(buf, l, 2, 4) : fmt_num(buf, (uint16_t)((l + 5) / 10), 1, 4); 
   e[0] = 'u'; e[1] = 'H'; e[2] = 0;
   disp_bold(1, 60, buf);

   // Capacitance value
   e = fmt_num(buf, c, 0, 4); 
    e[0] = 'p'; e[1] = 'F'; e[2] = 0;
   disp_bold(11, 60, buf);

   // ANT / TX indicator
   disp_bold(6, 106, r->sw ? "ANT" : " TX");
}

static uint8_t shown_eff = 0xFF;

static void show_efficiency(uint8_t pct) {
   if (pct == shown_eff) return;
   shown_eff = pct;
   disp_eff_bar(BAR_Y, pct);
}

static void show_screen(void) {
   oled_clear();
   disp_bold(1, 0, "L");
   disp_bold(11, 0, "C");
   shown_rel = 0xFFFF;
   show_relays(&rel);
   show_swr_label();
   disp_med(LINE3, 0, "PWR");
   disp_med(LINE3, 42, "=");
   disp_med(LINE3, 96, "W");
   disp_med(EFF_Y, 0, "EFF");
   shown_pwr = shown_swr = 0xFFFF;
   shown_eff = 0xFF;
   show_efficiency(0);
}

static void show_power(uint16_t p10) {        // 0.1 W (0..1500 = 0.0..150.0 W)
   if (p10 == shown_pwr) return;
   shown_pwr = p10;

   if (p10 < 100) {
      // 0.0 W to 9.9 W: safe for fmt_num() since p10 < 100
      fmt_num(buf, p10, 1, 3);
   } else {
      // 10 W to 999 W: format directly into 3 right-aligned characters to avoid 16-bit overflow
      uint16_t w = (uint16_t)((p10 + 5u) / 10u);
      if (w > 999u) w = 999u;

      buf[0] = (w >= 100u) ? (char)('0' + (w / 100u)) : ' ';
      buf[1] = (char)('0' + ((w / 10u) % 10u));
      buf[2] = (char)('0' + (w % 10u));
      buf[3] = 0;
   }

   // Always render big power digits on LINE3
   disp_med(LINE3, 60, buf);
}

static void show_swr(uint16_t swr) {          // SWR x 100, 0 = none
   if(swr == shown_swr || msg_on) return;
   shown_swr = swr;

   if(swr) {
      disp_med(LINE2, 60, num(swr, 2, 4));

      // Calculate mismatch efficiency % = (4 * SWR) / (SWR + 1)^2 * 100
      // Using swr scaled by 100 (e.g., 100 = 1.00):
      uint32_t num_eff = 40000UL * swr;
      uint32_t den_eff = (uint32_t)(swr + 100u) * (swr + 100u);
      uint8_t eff_pct = (uint8_t)((num_eff + den_eff / 2u) / den_eff);
      show_efficiency(eff_pct);
   } else {
      disp_med(LINE2, 60, "-.--");           // none measured yet
      show_efficiency(0);                     // clear bar when idle
   }
}

// a message in place of the SWR line for 'ms'
static void message(const char *s, uint16_t ms) {
   disp_med(LINE2, 0, "          ");
   disp_med(LINE2, 0, s);
   msg_on = 1;
   t_msg = tick_ms() + ms;
}

static void message_end(void) {
   msg_on = 0;
   disp_med(LINE2, 0, "          ");
   show_swr_label();
   shown_swr = 0xFFFF;
   shown_eff = 0xFF;
}

static void interface_init(void) {
   if(cfg[CFG_EXT] == INTERFACE_UART) {
      icom_interface_suspend();
      UART_init();
   }
   else {
      UART_suspend();
      icom_interface_init();
   }
}

// ---------------------------------------------------------------- tuning

static void save_state(void) {
   st.r = rel;
   state_save();
   mem_save();
}

// called by tune.c
void hal_progress(const relays_t *best, uint16_t swr) {
   msg_on = 0;
   show_relays(best);
   show_swr(swr);
}
uint8_t hal_abort(void) {
   uint8_t ev = buttons_event();
   if(ev == EV_NONE && cfg[CFG_EXT] == INTERFACE_ICOM)
      ev = icom_interface_event();
   // power off, or the transceiver's bypass pulse: after the stop (a tune
   // request from the transceiver while tuning changes nothing)
   if(ev == EV_TUNE_XLONG) {
      buttons_unget(ev);
      return 1;
   }
   return ev == EV_TUNE_SHORT || ev == EV_TUNE_LONG || ev == EV_EXT_SHORT;
}

// Carrier power for tuning, the same limits as in tune_run: P_OK or why
// not (P_NONE below 0.1 W, P_LOW below Cell 4 unless the SWR is 1.6 or
// more, P_HIGH above Cell 5)
enum { P_OK, P_NONE, P_LOW, P_HIGH };
static uint8_t tune_power(const meas_t *m) {
   if(m->pf < SWR_SHOW_UW) return P_NONE;
   if(!POWER_OK(m, TUNE_MIN_UW, FOLD_G2_START)) return P_LOW;
   if(pnet_uw(m) > TUNE_MAX_UW) return P_HIGH;
   return P_OK;
}

// two small lines right of TUNE, where the SWR so far follows
static void hint(const char *a, const char *b) {
   // y = 16 (Page 2) and y = 24 (Page 3): stays completely clear of PWR on Page 4 (y = 32..39)!
   disp_small(LINE2 - 3, 60, "        ");
   disp_small(LINE2 + 5, 60, "        ");
   disp_small(LINE2 - 3, 60, a); 
   disp_small(LINE2 + 5, 60, b); 
}

// Waits up to CARRIER_MS for a carrier the tune can use; after HINT_MS
// without one the display says why, and the power line shows the power
// meanwhile. A carrier that is there at once starts the tune at once
// (auto tune has checked it already); one that comes while waiting must
// last CARRIER_ON_MS, else a blip or a CW element would start a tune that
// waits again in tune_run, without a hint. So must one whose first
// reading is unsteady: on a key edge the SWR reads too high and may let
// a carrier below Cell 4 pass (from SWR 1.6 on 0.1 W is enough). TUNE_OK: there is one, else
// TUNE_NO_CARRIER (*why: the reason shown last) or TUNE_ABORTED (button,
// as during the tune).
static uint8_t wait_carrier(uint8_t *why) {
   meas_t m;
   uint8_t p, shown = P_OK, waited = 0, r = TUNE_OK;
   uint16_t t = tick_ms(), t_bad = t;
   for(;;) {
      meas_take(&m, 4);
      p = tune_power(&m);
      if(p != P_OK || (!waited && !m.stable)) {
         waited = 1;
         t_bad = tick_ms();
      }
      else if(!waited || since(t_bad) >= CARRIER_ON_MS) break;
      show_power(pwr_x10(m.pf));
      if(hal_abort()) { r = TUNE_ABORTED; break; }
      if(p != P_OK) {                          // a carrier on its way: the hint stays
         if(since(t) >= CARRIER_MS) {
            r = TUNE_NO_CARRIER;
            *why = shown;
            break;
         }
         // once a power was there, its hint stays when it goes away
         // (CW keying, a carrier near 0.1 W): no flicker with WAITING
         if(since(t) >= HINT_MS && p != shown && (p != P_NONE || shown == P_OK)) {
            shown = p;
            if(p == P_NONE) hint("WAITING", "FOR RF");
            else hint("POWER", p == P_LOW ? "TOO LOW" : "TOO HIGH");
         }
      }
      CLRWDT();
      delay_ms(10);
   }
   if(shown != P_OK) {
      hint("", "");
      shown_pwr = 0xFFFF;
   }
   return r;
}

static void do_tune(uint8_t by_auto) {
   uint8_t r, same, why = P_NONE;
   char tunestr[10] = "TUNE      ";
   relays_t from = rel;
   icom_interface_tuning(1);
   tunestr[5] = (icom_interface_active() && ICOM_START) ? '*' : ' ';
   msg_on = 0;
   disp_med(LINE2, 0, tunestr);                 // the SWR so far follows at the right
   shown_swr = 0xFFFF;                         // blanked: draw it even if unchanged
   r = wait_carrier(&why);
   if(r == TUNE_OK) {
      tune_resume = (resume == RESUME_ANY || (resume == RESUME_TUNE && !by_auto))
                    && since(t_tuned) < RESUME_MS ? resume : 0;
      // in bypass the relays hold no tune result (the memory has it)
      r = tune_run(&rel, !st.bypass && st.last_swr ? (uint16_t)(100 + st.last_swr) : 0);
      // Carrier gone while the search still measured new settings: the
      // next tune within RESUME_MS goes on with this search, and auto tune
      // starts it with the next carrier (SWR above 1.20). After TOO LOW /
      // TOO HIGH only TUNE goes on with it (the power changed meanwhile);
      // auto tune would end the same way. The step budget counts on, so
      // the chain ends.
      resume = tune_resumable;
   }
   else resume = 0;                       // nothing measured: as a tune without a carrier
   same = rel.l == from.l && rel.c == from.c && rel.sw == from.sw;
   if(r != TUNE_OK && r != TUNE_NO_MATCH && same) {
      // Stopped without a change (no carrier, or stopped before anything
      // better was found, e.g. by the long press that goes on to power
      // off): everything stays as it was, the bypass too, and nothing
      // is written to the EEPROM. Auto tune does not start the same tune
      // again at the present SWR: it would end the same way (stopped, too
      // much power, or too little carrier to find anything better)
      if(swr_last) swr_ref = swr_last;
   }
   else {
      // a result ends the bypass
      if(r == TUNE_OK || r == TUNE_NO_MATCH || rel.l || rel.c || rel.sw) st.bypass = 0;
      // last_swr: SWR - 1.00 in hundredths, 1..255 (0 = no tune result,
      // so a perfect 1.00 is kept as 1.01)
      if(!st.bypass)
         st.last_swr = (r == TUNE_OK && (rel.l || rel.c))
                       ? (uint8_t)(tune_swr - 100 > 255 ? 255 : tune_swr > 101 ? tune_swr - 100 : 1) : 0;
      swr_ref = tune_swr;
      swr_last = swr_disp = tune_swr;
      CLRWDT();
      save_state();
   }
   if(resume == RESUME_ANY) swr_ref = 0;       // auto tune goes on with the search
   show_relays(&rel);                          // the progress showed the best so far
   show_swr_label();
   shown_swr = 0xFFFF;
   show_swr(swr_disp);
   if(r == TUNE_TOO_LOW || (r == TUNE_NO_CARRIER && why == P_LOW)) message("TOO LOW", 2000);
   else if(r == TUNE_TOO_HIGH || (r == TUNE_NO_CARRIER && why == P_HIGH)) message("TOO HIGH", 2000);
   else if(r == TUNE_NO_CARRIER) message("NO POWER", 2000);
   else if(r == TUNE_NO_MATCH) message("NO MATCH", 2000);
   else if(r == TUNE_ABORTED) message("STOP", 1000);
   else if(r == TUNE_OVERLOAD) message("OVERLOAD", 2000);
   icom_interface_tuning(0);
   auto_cnt = 0;
   t_active = t_tuned = tick_ms();
}

static void bypass_toggle(uint8_t on) {
   if(on && !st.bypass) {                      // keep the setting to come back to
      st.byp = rel;
      st.bypass = 1;
      relays_set(0, 0, 0);
   }
   else if(!on && st.bypass) {
      st.bypass = 0;
      relays_set(st.byp.l, st.byp.c, st.byp.sw);
   }
   else return;
   save_state();
   swr_ref = st.bypass ? 0 : (uint16_t)(st.last_swr ? 100 + st.last_swr : 0);
   show_relays(&rel);
   show_swr_label();
   message(st.bypass ? "BYPASS" : "TUNED", 800);
   auto_cnt = 0;
}

static void relay_setting_changed(void) {
   if(!st.bypass) relays_set(rel.l, rel.c, rel.sw);
   st.last_swr = 0;
   swr_ref = swr_last = swr_disp = 0;
   resume = tune_resume = tune_resumable = 0;
   auto_cnt = 0;
   t_tuned = tick_ms();
   save_state();
   show_relays(&rel);
   show_swr_label();
   shown_swr = 0xFFFF;
   show_swr(0);
}

// These functions return 1 if the setting changed, else 0.
static uint8_t set_inductance_relays(uint8_t value) {
   relays_t *setting = st.bypass ? &st.byp : &rel;
   if(value > L_MAX || value == setting->l) return 0;
   setting->l = value;
   return 1;
}
static uint8_t set_capacitance_relays(uint8_t value) {
   relays_t *setting = st.bypass ? &st.byp : &rel;
   if(value > C_MAX || value == setting->c) return 0;
   setting->c = value;
   return 1;
}
static uint8_t set_cap_side(uint8_t side) {
   relays_t *setting = st.bypass ? &st.byp : &rel;
   side = side ? 1 : 0;
   if(setting->sw == side) return 0;
   setting->sw = side;
   return 1;
}

static void uart_reply_decimal(uint16_t value) {
   uint16_t divisor;
   uint8_t started = 0;

   for(divisor = 1000; divisor; divisor /= 10) {
      uint8_t digit = (uint8_t)(value / divisor);
      value %= divisor;
      if(started || digit) {
         UART_Write_Char((char)('0' + digit));
         started = 1;
      }   
   }   
   if(!started) UART_Write_Char('0');
   UART_Write_Char(';');
}

static void uart_reply_relay_mask(uint8_t inductance) {
   relays_t *setting = st.bypass ? &st.byp : &rel;
   uart_reply_decimal(inductance ? setting->l : setting->c);
}

static void uart_reply_cap_position(void) {
   relays_t *setting = st.bypass ? &st.byp : &rel;
   UART_Write_Char(setting->sw ? 'L' : 'C');
   UART_Write_Char(setting->sw ? 'C' : 'L');
   UART_Write_Char(';');
}

static void uart_reply_flag(uint8_t f) {
   UART_Write_Char(f ? '+' : '-');
   UART_Write_Char(';');
}

// ---------------------------------------------------------------- power

// centred on the 128 px: big text 12 px, small text 6 px per character,
// without the gap after the last one
#define CENTRE_BIG(s)   ((128 - 12 * (sizeof(s) - 1) + 2) / 2)
#define CENTRE_SMALL(s) ((128 - 6 * (sizeof(s) - 1) + 1) / 2)
#define GREET_FW "NG " FW_VERSION

// two pages of 2 s: the hardware, then the firmware and its version
static void greeting(void) {
   oled_clear();
   disp_big(LINE1, CENTRE_BIG("ATU-100"), "ATU-100");
   disp_small(17, CENTRE_SMALL("HARDWARE BY N7DDC"), "HARDWARE BY N7DDC");
   disp_big(27, CENTRE_BIG(GREET_FW), GREET_FW);
   disp_small(46, CENTRE_SMALL("FIRMWARE BY DL8UG"), "FIRMWARE BY DL8UG");
   disp_small(58, CENTRE_SMALL("PORT BY N1GP"), "PORT BY N1GP");
   delay_ms(2500);
}

static void start_screen(void) {
   greeting();
   if(btn_tune_held >= SETUP_HOLD) {
      setup_run();                             // held through the greeting
      interface_init();
   }
   show_screen();
   buttons_clear();                            // the press that woke us is no event
   icom_interface_clear();
   t_active = tick_ms();
}

// ---------------------------------------------------------------- watching

static void watch(void) {
   meas_t m;
   uint16_t p10, swr, delta;
   uint16_t now = tick_ms();
   uint8_t pw;
   meas_take(&m, 4);
   if(m.overflow && !msg_on) message("OVERLOAD", 2000);   // before RECHARGE
   p10 = pwr_x10(m.pf);
   // peak hold for the power display (Cell 10)
   if(m.pf >= peak.pf || now - t_peak >= (uint32_t)cfg[CFG_PEAK] * 10) {
      peak = m;
      t_peak = now;
   }
   swr = swr_x100(m.g2);
   // power compared as in tune_run (not the rounded p10), else auto tune
   // starts tunes that never see a carrier
   pw = tune_power(&m);
   // only a measurement whose halves agree: one on a key edge (carrier
   // coming or going between F1 and F2) gives a wrong SWR, which would
   // stay on the display until the next one. The display skips one with
   // a clipped detector too (wrong Pr / Pf); swr_last takes it, auto tune
   // compares the same readings with it (no new tune after OVERLOAD)
   if(m.stable) {
      if(pw == P_OK || pw == P_HIGH) swr_last = swr;
      if(pw != P_NONE && !m.overflow) swr_disp = swr;
      swr_live = pw != P_NONE ? swr : 0;
      pwr_live = pw != P_NONE ? p10 : 0;
   }

   // auto tune: power for tuning, SWR above 1.20 and changed by more than
   // Cell 6 since the last tune (not again right after a tune, and not
   // again and again when the antenna cannot be matched better)
   delta = (uint16_t)(cfg[CFG_AUTO_DELTA] - 10) * 10;

   if(!(icom_interface_active() && ICOM_START) && cfg[CFG_AUTO] && !st.bypass && pw == P_OK && m.stable
      && swr > 120 && since(t_tuned) >= AUTO_HOLD && (swr > swr_ref + delta || swr + delta < swr_ref)) {
      if(++auto_cnt >= AUTO_STEADY) auto_tune = 1;   // main loop starts it
   }
   else auto_cnt = 0;
}

static void show(void) {
   show_power(pwr_x10(peak.pf));
   show_swr(swr_disp);
}

// ---------------------------------------------------------------- main
void main(void) {
   uint8_t ev;
   // Keep all pins in safe high-impedance input state during ICSP verification gaps
   board_init();
   i2c_init();
   settings_load();
   interface_init();
   oled_init();
   meas_init();
   mem_load();
   if(state_load()) {
      rel = st.r;
      relays_set(st.r.l, st.r.c, st.r.sw);
   }
   else {
      st.bypass = 0;
      st.last_swr = 0;
      st.byp.l = st.byp.c = st.byp.sw = 0;
      relays_set(0, 0, 0);
   }
   swr_ref = st.last_swr ? (uint16_t)(100 + st.last_swr) : 0;
   start_screen();
   t_watch = t_show = t_refresh = tick_ms();
   extern uart_command_result_t uart_command;

   for(;;) {
      CLRWDT();
      if(cfg[CFG_EXT] == INTERFACE_UART) {
         uart_command = uart_command_poll();
         switch(uart_command.command) {
            case UART_COMMAND_BYPASS_OFF:
               bypass_toggle(0); uart_reply_flag(st.bypass); break;
            case UART_COMMAND_BYPASS_ON:
               bypass_toggle(1); uart_reply_flag(st.bypass); break;
            case UART_COMMAND_AUTO_OFF:
               message("AUTO-OFF", 800); cfg[CFG_AUTO] = 0; auto_cnt = 0;
               auto_tune = 0; uart_reply_flag(cfg[CFG_AUTO]); break;
            case UART_COMMAND_AUTO_ON:
               message("AUTO-ON", 800); cfg[CFG_AUTO] = 1;
               uart_reply_flag(cfg[CFG_AUTO]); break;
            case UART_COMMAND_AUTO_QUERY:    uart_reply_flag(cfg[CFG_AUTO]); break;
            case UART_COMMAND_L_SET:
               if(set_inductance_relays(uart_command.argument)) relay_setting_changed();
               uart_reply_relay_mask(1); break;
            case UART_COMMAND_C_SET:
               if(set_capacitance_relays(uart_command.argument)) relay_setting_changed();
               uart_reply_relay_mask(0); break;
            case UART_COMMAND_L_QUERY:       uart_reply_relay_mask(1); break;
            case UART_COMMAND_C_QUERY:       uart_reply_relay_mask(0); break;
            case UART_COMMAND_BYPASS_QUERY:    uart_reply_flag(st.bypass); break;
            case UART_COMMAND_CAP_LOAD:
               if(set_cap_side(1)) relay_setting_changed();
               uart_reply_relay_mask(1); break;
            case UART_COMMAND_CAP_SOURCE:
            if(set_cap_side(0)) relay_setting_changed();
            uart_reply_relay_mask(0); break;
            case UART_COMMAND_CAP_POSITION_QUERY: uart_reply_cap_position(); break;
            case UART_COMMAND_SWR_QUERY:     uart_reply_decimal(swr_live); break;
            case UART_COMMAND_LAST_SWR_QUERY:
               uart_reply_decimal(swr_disp); break;
            case UART_COMMAND_PEAK_POWER_QUERY: uart_reply_decimal(pwr_x10(peak.pf)); break;
            case UART_COMMAND_POWER_QUERY:      uart_reply_decimal(pwr_live); break;
            case UART_COMMAND_NONE:          break;
         }
      }

      ev = buttons_event();
      if(ev == EV_NONE && cfg[CFG_EXT] == INTERFACE_ICOM)
         ev = icom_interface_event();
      switch(ev) {
         case EV_TUNE_SHORT:     bypass_toggle(!st.bypass); break;
         case EV_TUNE_LONG:      do_tune(0); break;
         case EV_BYP:            bypass_toggle(!st.bypass); break;
         case EV_AUTO:
             if (cfg[CFG_AUTO]) {
                 message("AUTO-OFF", 800);
                 cfg[CFG_AUTO] = 0;
                 auto_cnt = 0;
                 auto_tune = 0;
             } else {
                 message("AUTO-ON", 800);
                 cfg[CFG_AUTO] = 1;
             }
             break;
         case EV_EXT_SHORT: bypass_toggle(1); break;
         case EV_EXT_LONG:  do_tune(0); break;
      }
      if(since(t_watch) >= WATCH_MS) {
         t_watch = tick_ms();
         watch();
      }
      if(auto_tune) {
         auto_tune = 0;
         do_tune(1);
      }
      if(since(t_show) >= SHOW_MS) {
         t_show = tick_ms();
         show();
      }
      if(msg_on && (int16_t)(tick_ms() - t_msg) >= 0) message_end();
      if(since(t_refresh) >= REFRESH_MS) {
         t_refresh = tick_ms();
      }
   }
}

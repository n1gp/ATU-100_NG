#include "board.h"
#include "timer.h"
#include "buttons.h"

#define SHORT_MIN   3
#define LONG_T      25
#define XLONG_T     250

static uint8_t long_tune_sent, long_auto_sent, long_byp_sent, xlong_tune_sent, pending;

void buttons_unget(uint8_t ev) {
   pending = ev;
}

void buttons_clear(void) {
   INTCONbits.GIE = 0;
   btn_tune_released = 0;
   btn_auto_released = 0;
   btn_byp_released = 0;
   INTCONbits.GIE = 1;
   pending = EV_NONE;
   long_tune_sent = xlong_tune_sent = long_byp_sent = long_auto_sent = 1;      // a press still going on gives no event
}

uint8_t buttons_event(void) {
   uint8_t ht, relt, ha, rela, hb, relb, eh, erel, ev;
   if(pending) {
      ev = pending;
      pending = EV_NONE;
      return ev;
   }
   INTCONbits.GIE = 0;
   ht = btn_tune_held;
   relt = btn_tune_released;
   btn_tune_released = 0;
   ha = btn_auto_held;
   rela = btn_auto_released;
   btn_auto_released = 0;
   hb = btn_byp_held;
   relb = btn_byp_released;
   btn_byp_released = 0;
   INTCONbits.GIE = 1;

   ev = EV_NONE;
   if(relt) {                        // a press has ended
      // if the main loop was busy the whole press long, judge by its length
      if(!long_tune_sent && relt >= XLONG_T) ev = EV_TUNE_XLONG;
      else if(!long_tune_sent && relt >= LONG_T) ev = EV_TUNE_LONG;
      else if(!long_tune_sent && relt >= SHORT_MIN) ev = EV_TUNE_SHORT;
      long_tune_sent = xlong_tune_sent = 0;
   }
   if(ht >= LONG_T && !long_tune_sent) {
      long_tune_sent = 1;
      if(ev) pending = EV_TUNE_LONG; else ev = EV_TUNE_LONG;
   }
   if(ht >= XLONG_T && !xlong_tune_sent) {
      xlong_tune_sent = 1;
      if(ev) pending = EV_TUNE_XLONG; else ev = EV_TUNE_XLONG;
   }
   if(ht == 0 && !relt) long_tune_sent = xlong_tune_sent = 0;

   if(relb) {
      if(relb >= SHORT_MIN) ev = EV_BYP;
   }
   if(rela) {
      if(rela >= SHORT_MIN) ev = EV_AUTO;
   }
   return ev;
}

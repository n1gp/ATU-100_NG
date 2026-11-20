// Setup menu on the device.
//
// Entered when the button is still held after the greeting (keep it
// pressed when switching on). Then:
//   short press  next value of the setting shown
//   long press   next setting; after the last: SAVE, HEX VALUES, EXIT
// On SAVE / HEX VALUES / EXIT a short press does it. Without a press for
// 60 s the menu ends without saving.

#include "board.h"
#include "timer.h"
#include "cells.h"
#include "settings.h"
#include "buttons.h"
#include "display.h"
#include "oled.h"
#include "setup.h"
#include "text.h"

#define TIMEOUT_MS 30000

enum { U_MS, U_W10, U_W, U_DSWR, U_ONOFF, U_DIV10, U_CALA, U_X10MS, U_TARGET, U_EFFORT, U_TURNS, U_OLEDOFFX, U_OLEDOFFY, U_INTERFACE };

static const char *const names[CELL_COUNT] = {
   "RELAY PULSE", "MIN. TUNE POWER", "MAX. TUNE POWER", "AUTO TUNE: SWR CHANGE",
   "AUTO TUNE", "CALIBRATION B (1 W)", "CALIBRATION A (100 W)", "POWER PEAK HOLD",
   "TUNE TARGET SWR", "SEARCH EFFORT", "TURNS RATIO", "OLED OFFSET X", "OLED OFFSET Y", "EXT INTERFACE"
};

static const uint8_t units[CELL_COUNT] = {
   U_MS, U_W10, U_W, U_DSWR, U_ONOFF, U_DIV10, U_CALA, U_X10MS, U_TARGET, U_EFFORT,
   U_TURNS, U_OLEDOFFX, U_OLEDOFFY, U_INTERFACE
};

// values offered per setting, ascending (other values from the hex file
// are shown and left when stepping on)
static const uint8_t v_relay[]  = {3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 20, 25, 30};
static const uint8_t v_minp[]   = {10, 15, 20, 30, 50};
static const uint8_t v_maxp[]   = {10, 12, 15, 25, 50, 75, 99};
static const uint8_t v_delta[]  = {11, 12, 13, 15, 20, 25, 30};
static const uint8_t v_onoff[]  = {0, 1};
static const uint8_t v_calb[]   = {2, 3, 4, 5, 6, 7, 8, 10, 12, 15, 20, 25, 30};
static const uint8_t v_cala[]   = {4, 6, 8, 10, 12, 14, 16, 18, 20, 25, 30, 40, 50, 60, 70};
static const uint8_t v_peak[]   = {10, 20, 30, 40, 60, 80, 99};
static const uint8_t v_target[] = {0, 2, 3, 5, 8, 10, 15, 20};
static const uint8_t v_effort[] = {1, 2, 3};
static const uint8_t v_turns[]  = {5, 10};
static const uint8_t v_oledxoff[] = {0, 1, 2};
static const uint8_t v_oledyoff[] = {0, 1, 2};
static const uint8_t v_interface[] = {INTERFACE_ICOM, INTERFACE_UART};

static const uint8_t *const lists[CELL_COUNT] = {
   v_relay, v_minp, v_maxp, v_delta, v_onoff, v_calb, v_cala, v_peak,
   v_target, v_effort, v_turns, v_oledxoff, v_oledyoff, v_interface
};
static const uint8_t list_len[CELL_COUNT] = {
   sizeof v_relay, sizeof v_minp, sizeof v_maxp, sizeof v_delta,
   sizeof v_onoff, sizeof v_calb, sizeof v_cala, sizeof v_peak, sizeof v_target,
   sizeof v_effort, sizeof v_turns, sizeof v_oledxoff, sizeof v_oledyoff, sizeof v_interface
};

static char txt[11];

static char *fmt(char *p, uint16_t v, uint8_t dec) {
   return fmt_num(p, v, dec, 0);
}

static void cat(char *p, const char *s) {
   while((*p++ = *s++)) continue;
}

enum { S_NONE, S_MS, S_W, S_TURNS, S_ONOFF, S_TARGET, S_EFFORT, S_INT };

typedef struct {
   uint8_t mode;   // S_NONE, S_MS, S_W, S_TURNS, S_ONOFF, S_TARGET, S_EFFORT, S_INT
   uint8_t dec;    // decimal places for fmt_num (0, 1, or 2)
   int8_t  bias;   // 0, -10 (for U_DSWR), or 100 (for U_CALA / U_TARGET)
} fmt_spec_t;

static const fmt_spec_t specs[CELL_COUNT] = {
   { S_MS,     0,   0 }, // 0: RELAY PULSE
   { S_W,      1,   0 }, // 1: MIN. TUNE POWER
   { S_W,      0,   0 }, // 2: MAX. TUNE POWER
   { S_NONE,   1, -10 }, // 3: AUTO TUNE: SWR CHANGE (v - 10, 1 dec)
   { S_ONOFF,  0,   0 }, // 4: AUTO TUNE
   { S_NONE,   1,   0 }, // 5: CALIBRATION B (1 W)
   { S_NONE,   2, 100 }, // 6: CALIBRATION A (100 W) (100 + v, 2 dec)
   { S_MS,     0,   0 }, // 7: POWER PEAK HOLD (handled with *10 below)
   { S_TARGET, 2, 100 }, // 8: TUNE TARGET SWR
   { S_EFFORT, 0,   0 }, // 9: SEARCH EFFORT
   { S_TURNS,  0,   0 }, // 10: TURNS RATIO
   { S_NONE,   0,   0 }, // 11: OLED OFFSET X
   { S_NONE,   0,   0 }, // 13: OLED OFFSET Y
   { S_INT,    0,   0 }  // 14: INTERFACE TYPE
};

static const char *const suffixes[] = { "", " MS", " W", " TURNS" };

static void value_text(uint8_t i, uint8_t v) {
   const fmt_spec_t *sp = &specs[i];
   uint8_t m = sp->mode;

   if (m == S_ONOFF) {
      cat(txt, v ? "ON" : "OFF");
   } else if (m == S_EFFORT) {
      cat(txt, v == 1 ? "QUICK" : v == 3 ? "THOROUGH" : "NORMAL");
   } else if (m == S_TARGET && v == 0) {
      cat(txt, "FULL");
   } else if (m == S_INT) {
      cat(txt, v == 1 ? "UART" : "ICOM");
   } else {
      uint16_t val = (i == 7) ? (uint16_t)v * 10u : (uint16_t)((int16_t)v + sp->bias);
      char *end = fmt(txt, val, sp->dec);
      if (m <= S_TURNS) cat(end, suffixes[m]);
   }
}


#define PAGE_SAVE  CELL_COUNT
#define PAGE_HEX   (CELL_COUNT + 1)
#define PAGE_EXIT  (CELL_COUNT + 2)
#define PAGES      (CELL_COUNT + 3)

static void show(uint8_t page) {
   char head[8], *p;
   oled_clear();
   if(page < CELL_COUNT) {
      p = fmt(head, (uint16_t)(page + 1), 0);
      *p++ = '/';
      fmt(p, CELL_COUNT, 0);
      disp_small(0, 0, head);
      disp_small(8, 0, names[page]);
      value_text(page, cfg[page]);
      disp_big(LINE2, 0, txt);
   }
   else {
      disp_small(0, 0, "SHORT PRESS:");
      disp_big(LINE2, 0, page == PAGE_SAVE ? "SAVE" : page == PAGE_HEX ? "HEX VALUES" : "EXIT");
   }
}

static uint8_t next_value(uint8_t i, uint8_t v) {
   const uint8_t *l = lists[i];
   uint8_t k;
   for(k = 0; k < list_len[i]; k++)
      if(l[k] > v) return l[k];
   return l[0];                       // wrap around
}

void setup_run(void) {
   uint8_t page = 0, ev;
   uint16_t t = (uint16_t)tick_ms();
   oled_clear();
   disp_big(LINE1, 34, "SETUP");
   while(BUTTON_TUNE_DOWN) CLRWDT();       // let go first
   buttons_clear();
   show(page);
   for(;;) {
      CLRWDT();
      ev = buttons_event();
      if(ev == EV_NONE) {
         if(((uint16_t)tick_ms() - t) >= TIMEOUT_MS) break;
         continue;
      }
      t = (uint16_t)tick_ms();
      if(ev == EV_TUNE_LONG) {
         page = (uint8_t)((page + 1) % PAGES);
         show(page);
         while(BUTTON_TUNE_DOWN) CLRWDT();
         buttons_clear();
      }
      else if(ev == EV_TUNE_SHORT) {
         if(page < CELL_COUNT) {
            cfg[page] = next_value(page, cfg[page]);
            show(page);
         }
         else {
            if(page == PAGE_SAVE) settings_save();
            if(page == PAGE_HEX) settings_defaults();
            break;
         }
      }
   }
   settings_load();                   // without SAVE: back to the values in effect
}

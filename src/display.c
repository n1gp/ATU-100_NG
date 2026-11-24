#include "hal.h"
#include "display.h"
#include "oled.h"
#include "i2c_soft.h"
#include "font5x8.h"
#include "timer.h"

#define W      128
#define PAGES  8

void disp_service(void){};
void disp_flush(void){};
void disp_refresh(void){};

// removed most lowercase from font5x8.h to save space
static const uint8_t *glyph(char ch) {
   if (ch == 'u') ch = '[';   // Redirect 'u' (uH) to '[' slot
   if (ch == 'p') ch = '\\';  // Redirect 'p' (pF) to '\' slot
   uint8_t i = (uint8_t)((uint8_t)ch - FONT_FIRST);
   if (i >= FONT_COUNT) i = 0;
   return &font5x8[i * 5];
}

static uint8_t dbl(uint8_t n) {
   uint8_t r = 0, i;
   for (i = 0; i < 4; i++)
      if (n & (1 << i)) r |= (uint8_t)(3 << (2 * i));
   return r;
}

void disp_cells(uint8_t y, uint8_t x, uint8_t bits) {
    uint8_t p = y >> 3;
    uint8_t i, k, col;
    
    // Page 0 uses bits 0..6 (0x7F / 0x41) to leave y=7 blank
    // Page 1 uses bits 1..7 (0xFE / 0x82) to leave y=8 blank
    uint8_t fill_mask   = (p == 0) ? 0x7F : 0xFE;
    uint8_t border_mask = (p == 0) ? 0x41 : 0x82;

    for (i = 0; i < 7; i++, bits >>= 1) {
        for (k = 0; k < 7; k++, x++) {
            if (k >= 6) {
                col = 0x00; // 1-pixel horizontal spacer between boxes
            } else if ((bits & 1) || k == 0 || k == 5) {
                col = fill_mask;
            } else {
                col = border_mask;
            }
            oled_write(p, x, &col, 1);
        }
    }
}

void disp_big(uint8_t y, uint8_t x, const char *s) {
   uint8_t p = y >> 3;
   uint8_t shift = y & 7;
   uint8_t i, col;
   uint8_t b0, b1, b2;
   const uint8_t *g;

   for (; *s; s++, x += 12) {
      g = glyph(*s);
      for (i = 0; i < 12; i++) {
         col = i < 10 ? g[i / 2] : 0;
         uint8_t lo = dbl(col & 0x0F);
         uint8_t hi = dbl(col >> 4);

         uint32_t w = ((uint32_t)lo | ((uint32_t)hi << 8)) << shift;
         b0 = (uint8_t)(w & 0xFF);
         b1 = (uint8_t)((w >> 8) & 0xFF);
         b2 = (uint8_t)((w >> 16) & 0xFF);

         oled_write(p, (uint8_t)(x + i), &b0, 1);
         if (p + 1 < PAGES) oled_write((uint8_t)(p + 1), (uint8_t)(x + i), &b1, 1);
         if (shift && p + 2 < PAGES) oled_write((uint8_t)(p + 2), (uint8_t)(x + i), &b2, 1);
      }
   }
}

// Bold 6x8 text: sends 7 columns in 1 burst per character
void disp_bold(uint8_t y, uint8_t x, const char *s) {
   uint8_t p = y >> 3;
   uint8_t i;
   uint8_t buf[7];
   const uint8_t *g;
   for (; *s; s++, x += 7) {
      g = glyph(*s);
      buf[0] = g[0];
      for (i = 1; i < 5; i++) buf[i] = (uint8_t)(g[i] | g[i - 1]);
      buf[5] = g[4];
      buf[6] = 0;
      oled_write(p, x, buf, 7);
   }
}

// Stretches an 8-bit glyph column into 12 vertical pixels (1.5x vertical scale)
static uint16_t stretch12(uint8_t b) {
   uint16_t r = 0;
   uint8_t i;
   for (i = 0; i < 4; i++, b >>= 2) {
      r >>= 3;
      if (b & 1) r |= 0x0600;
      if (b & 2) r |= 0x0800;
   }
   return r;
}

void disp_med(uint8_t y, uint8_t x, const char *s) {
   uint8_t p = y >> 3;
   uint8_t shift = y & 7;
   uint8_t i;
   uint8_t b0, b1;
   const uint8_t *g;

   for (; *s; s++, x += 11) {
      g = glyph(*s);
      for (i = 0; i < 11; i++) {
         uint16_t col = (i < 10) ? stretch12(g[i / 2]) : 0;
         uint16_t w = (uint16_t)(col << shift);
         b0 = (uint8_t)(w & 0xFF);
         b1 = (uint8_t)((w >> 8) & 0xFF);

         oled_write(p, (uint8_t)(x + i), &b0, 1);
         if (p + 1 < PAGES) oled_write((uint8_t)(p + 1), (uint8_t)(x + i), &b1, 1);
      }
   }
}

// Small 5x8 text: supports any y pixel offset (0..56) across 1 or 2 OLED pages
void disp_small(uint8_t y, uint8_t x, const char *s) {
   uint8_t p = y >> 3;
   uint8_t shift = y & 7;
   uint8_t i;
   uint8_t top[6], bot[6];
   const uint8_t *g;

   for (; *s; s++, x += 6) {
      g = glyph(*s);
      for (i = 0; i < 5; i++) {
         uint16_t w = (uint16_t)g[i] << shift;
         top[i] = (uint8_t)(w & 0xFF);
         bot[i] = (uint8_t)(w >> 8);
      }
      top[5] = 0;
      bot[5] = 0;

      oled_write(p, x, top, 6);
      if (shift && p + 1 < PAGES) {
         oled_write((uint8_t)(p + 1), x, bot, 6);
      }
   }
}

void disp_eff_bar(uint8_t y, uint8_t pct) {
   uint8_t p = y >> 3;
   uint8_t i, fill, col;
   if (pct > 100) pct = 100;
   fill = (uint8_t)(((uint16_t)pct * 88u + 50u) / 100u);

   for (i = 0; i < 92; i++) {
      if (i == 0 || i == 91) {
         col = 0xFF;
      } else if (i >= 2 && i < (uint8_t)(2 + fill)) {
         col = 0xBD;
      } else {
         col = 0x81;
      }
      oled_write(p, (uint8_t)(36 + i), &col, 1);
   }
}

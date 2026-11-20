// Display: everything is drawn into a framebuffer in RAM; only the changed
// parts go to the display, a little at a time from the main loop, and never
// while relays switch or the detectors are measured.

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define LINE1  0     // Page 0 (y=0..7): L relays + uH, Page 1 (y=8..15): C relays + pF
#define LINE2  20    // SWR line (16 px high: y = 22..37)
#define LINE3  37    // PWR line (16 px high: y = 39..54)
#define EFF_Y  53    // EFF label top row (12 px high: y = 52..63)
#define BAR_Y  56    // Bottom efficiency bar (8 px high: y = 56..63)

#define LINE_MID 30

void disp_bold(uint8_t y, uint8_t x, const char *s);
void disp_big(uint8_t y, uint8_t x, const char *s);        // 12 px per character
void disp_med(uint8_t y, uint8_t x, const char *s);
void disp_small(uint8_t y, uint8_t x, const char *s);      // 6 px per character, from pixel row y
void disp_cells(uint8_t y, uint8_t x, uint8_t bits);
void disp_eff_bar(uint8_t y, uint8_t pct);

#endif

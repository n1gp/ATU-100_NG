// Display: everything is drawn into a framebuffer in RAM; only the changed
// parts go to the display, a little at a time from the main loop, and never
// while relays switch or the detectors are measured.

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

#define LINE1  0     // L at y=1..8, C at y=10..17
#define LINE2  18    // SWR line (16 px high: y = 19..34)
#define LINE3  36    // PWR line (16 px high: y = 37..52)
#define EFF_Y  52    // EFF label (8 px high via disp_bold: y = 55..62)
#define BAR_Y  56    // Bottom efficiency bar (8 px high: y = 56..63)

void disp_bold(uint8_t y, uint8_t x, const char *s);
void disp_big(uint8_t y, uint8_t x, const char *s);        // 12 px per character
void disp_med(uint8_t y, uint8_t x, const char *s);
void disp_small(uint8_t y, uint8_t x, const char *s);      // 6 px per character, from pixel row y
void disp_cells(uint8_t y, uint8_t x, uint8_t bits);
void disp_eff_bar(uint8_t y, uint8_t pct);
void disp_clear(void);
void disp_service(void);
void disp_flush(void);
void disp_refresh(void);

#endif

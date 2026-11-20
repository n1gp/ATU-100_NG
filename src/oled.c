// The display initialization sequence follows the ATU-10 firmware by
// David Fainitski, N7DDC (public domain).

#include "board.h"
#include "i2c_soft.h"
#include "cells.h"
#include "oled.h"

#define ADDR       0x78              // write address

static uint8_t commands(const uint8_t *c, uint8_t n) {
   uint8_t nack;
   i2c_start();
   nack = i2c_write(ADDR);
   i2c_write(0x00);                  // command stream
   while(n--) i2c_write(*c++);
   i2c_stop();
   return nack;
}

uint8_t oled_write(uint8_t page, uint8_t x, const uint8_t *d, uint8_t n) {
   uint8_t a[3], nack;
   x += cfg[CFG_OLEDX];
   a[0] = (uint8_t)(0xB0 + page);
   a[1] = x & 0x0F;
   a[2] = (uint8_t)(0x10 | (x >> 4));
   nack = commands(a, 3);
   i2c_start();
   nack |= i2c_write(ADDR);
   i2c_write(0x40);                  // data stream
   while(n--) i2c_write(*d++);
   i2c_stop();
   return nack;
}

void oled_clear(void) {
    uint8_t p, col;
    uint8_t a[3];

    for (p = 0; p < 8; p++) {
        a[0] = (uint8_t)(0xB0 + p);
        a[1] = 0x00;
        a[2] = 0x10;
        commands(a, 3);

        i2c_start();
        i2c_write(ADDR);
        i2c_write(0x40);              // data stream
        for (col = 0; col < 128; col++) {
            i2c_write(0x00);          // Write 0x00 to blank all pixels
        }
        i2c_stop();
    }
}

uint8_t oled_init(void) {
    uint8_t init_cmds[] = { 
        0xAE,       // Display OFF while initializing
        0xD5, 0x80, // Set Display Clock Divide Ratio
        0xA8, 0x3F, // Multiplex Ratio: 64 lines (0x3F)
        0xD3, cfg[CFG_OLEDY], // Display Offset: 0
        0x40,             // Display Start Line: 0
        0x8D, 0x14, // Charge Pump: ENABLE (0x14)
        0x20, 0x02, // Memory Addressing Mode: Page Addressing
        0xA1,       // Segment Re-map: Normal
        0xC8,       // COM Output Scan Direction: Normal
        0xDA, 0x12, // COM Pins Hardware Config: Alternative (0x12 for 128x64)
        0x81, 0xCF, // Contrast Control: High
        0xD9, 0xF1, // Pre-charge Period
        0xDB, 0x40, // VCOMH Deselect Level
        0xA4,       // Entire Display ON (Resume RAM content)
        0xA6        // Normal Display (Not Inverted)
    };  

    // Send Init Commands (Keep display OFF during RAM clear)
    if (commands(init_cmds, sizeof(init_cmds))) return 1;

    // Wipe GDDRAM while display is off
    oled_clear();

    // Turn Display ON cleanly
    uint8_t disp_on = 0xAF;
    if (commands(&disp_on, 1)) return 1;

    return 0;
}

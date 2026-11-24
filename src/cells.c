#include "cells.h"

uint8_t cfg[CELL_COUNT];

//                                      1   2   3   4   5   6   7   8   9  10  11  12  13  14
const uint8_t cell_min[CELL_COUNT] = {  2,  1,  1,  11, 0,  0,  0,  1,  0,  1,  5,  0,  0,  0 };
const uint8_t cell_max[CELL_COUNT] = {  30, 99, 99, 99, 1,  99, 99, 99, 99, 3,  10, 2,  2,  1 };
const uint8_t cell_def[CELL_COUNT] = {  10, 10, 99, 13, 1,  20, 25, 60, 5,  2,  10, 0,  0,  1 };

// volatile: the compiler must read the values from program memory at run
// time and not build them into the code, or editing the hex would not work
const volatile uint8_t Cells[CELLS_SIZE] __at(CELLS_ADDR) = {
   0x10,   // 1  relay pulse 10 ms (the HRS1H-S-12 relay data sheet asks for 5 ms)
   0x10,   // 2  tune from 1.0 W
   0x99,   // 3  tune up to 99 W
   0x13,   // 4  auto tune if the SWR changed by more than 0.3
   0x01,   // 5  auto tune on
   0x20,   // 6  calibration b = 0.4 (BAT41 detector diodes)
   0x25,   // 7  calibration a = 1.14 (BAT41)
   0x60,   // 8 peak hold 600 ms
   0x05,   // 9 tuning target SWR 1.05
   0x02,   // 10 search effort 2
   0x10,   // 11 tandem turns ratio of 10
   0x00,   // 12 offset rows to the left (horizontally move) for OLED displays
   0x00,   // 13 offset down (vertically move) for OLED displays
   0x01,   // 14 external interface: UART (default)
};

uint8_t cell_decode(uint8_t i, uint8_t bcd) {
   uint8_t v;
   if((bcd >> 4) > 9 || (bcd & 0x0F) > 9) return cell_def[i];
   v = (uint8_t)((bcd >> 4) * 10 + (bcd & 0x0F));
   if(v < cell_min[i] || v > cell_max[i]) return cell_def[i];
   return v;
}

void cells_load(void) {
   uint8_t i;
   for(i = 0; i < CELL_COUNT; i++) cfg[i] = cell_decode(i, Cells[i]);
}

// Fletcher-16 over the raw Cells: a settings block saved by the setup menu
// only applies while the hex Cells are the ones it was made with
uint16_t cells_hash(uint8_t n) {
   uint8_t i, a = 0x5A, b = 0xA5;
   for(i = 0; i < n; i++) {
      a = (uint8_t)((a + Cells[i]) % 255);
      b = (uint8_t)((b + a) % 255);
   }
   return (uint16_t)b << 8 | a;
}

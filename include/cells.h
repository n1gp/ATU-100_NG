// Settings ("Cells"): 16 BCD coded bytes at a fixed place in program
// memory, word address 0x7770 (byte address 0xEEE0 in the hex file), so
// they can be changed in the hex file without rebuilding the firmware.
// Decoding is free of hardware access and also builds on the PC.

#ifndef CELLS_H
#define CELLS_H

#include <stdint.h>

#ifdef __16F18857
#define CELLS_ADDR  0x7770
#else
#define CELLS_ADDR  0x3FF0
#endif
#define CELL_COUNT  14
// 16 words reserved, the Cells fill exactly two
// hex records (:10EEE000 and :10EEF000) that hold nothing else
#define CELLS_SIZE  16

enum {
   CFG_RELAY_MS,     // 1  relay pulse time in ms
   CFG_MIN_PWR,      // 2  minimum power for tuning in 0.1 W
   CFG_MAX_PWR,      // 3  maximum power for tuning in W
   CFG_AUTO_DELTA,   // 4  auto tune if the SWR changed by more than (value - 10) / 10
                     //    (from 90 on practically never: the SWR cannot change that much)
   CFG_AUTO,         // 5  auto tune 1 = on, 0 = off
   CFG_CAL_B,        // 6  detector calibration b = value / 10
   CFG_CAL_A,        // 7  detector calibration a = 1 + value / 100
   CFG_PEAK,         // 8 peak hold time for the power display in 10 ms
   CFG_TARGET,       // 9 tuning target: SWR 1 + value / 100, 0 = always full search
   CFG_SEARCH,       // 10 search effort 1..3
   CFG_TURNS,        // 11 ratio of turns of the tandem match
   CFG_OLEDX,        // 12 offset rows to the left (horizontally move) for OLED displays
   CFG_OLEDY,        // 13 offset down (vertically move) for OLED displays
   CFG_EXT,          // 11 external interface: 0 = ICOM, 1 = UART
};

enum { INTERFACE_ICOM, INTERFACE_UART };

extern uint8_t cfg[CELL_COUNT];                 // decoded, decimal
extern const uint8_t cell_min[CELL_COUNT], cell_max[CELL_COUNT], cell_def[CELL_COUNT];

uint8_t cell_decode(uint8_t i, uint8_t bcd);    // decimal value, default if invalid
void cells_load(void);                          // hex Cells -> cfg[]
uint16_t cells_hash(uint8_t n);                 // checksum of the first n hex Cells

#endif

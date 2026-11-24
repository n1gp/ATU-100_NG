// 1 ms system tick and raw sampling of the button and the external start
// line. The interrupt only counts; all decisions are made in buttons.c.

#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// press time in 10 ms units while held (saturates at 255), 0 = released
extern volatile uint8_t btn_tune_held, btn_auto_held, btn_byp_held;
// length of the last finished press in 10 ms units, 0 = none pending
extern volatile uint8_t btn_tune_released, btn_auto_released, btn_byp_released;

extern volatile uint8_t tx_shift;
extern volatile uint8_t tx_bits_left;
extern volatile uint8_t tx_subtick;

uint16_t tick_ms(void);                  // read atomically

#endif

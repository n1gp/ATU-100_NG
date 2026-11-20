// ATU-10 board: pin map and low level hardware set-up.
// This is the only place that knows which pin does what.
// Pin assignment of the ATU-10 hardware by David Fainitski, N7DDC.

#ifndef BOARD_H
#define BOARD_H

#ifdef __XC
#include <xc.h>
#endif
#include <stdint.h>

#define _XTAL_FREQ 32000000UL

// Button and external interface (both active low)
#define BUTTON_TUNE_DOWN   (!PORTBbits.RB0)
#define BUTTON_AUTO_DOWN   (!PORTBbits.RB1)
#define BUTTON_BYP_DOWN    (!PORTBbits.RB2)

// Display
// ====================================================================
// ATU-100 SOFTWARE I2C PIN REDEFINDITIONS (PINS 9 & 10)
// ====================================================================
#define I2C_SCL       TRISBbits.TRISB6   // 1 = Float High, 0 = Drive Low
#define I2C_SCL_IN    PORTBbits.RB6
#define I2C_SDA       TRISBbits.TRISB7   // 1 = Float High, 0 = Drive Low
#define I2C_SDA_IN    PORTBbits.RB7

// ADC channels
#define ADC_CH_FWD    1                    // forward detector, RA1
#define ADC_CH_REV    0                    // reverse detector, RA0

void board_init(void);
void delay_ms(uint16_t ms);      // run-time value, clears the watchdog

#endif

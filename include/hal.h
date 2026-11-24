// ATU-100 board: pin map and low level hardware set-up.
// This is the only place that knows which pin does what.

#ifndef HAL_H
#define HAL_H

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
#ifdef __16F18857
#define I2C_SCL       LATBbits.LATB6        // Physical Pin 27 (SCL)
#define I2C_SDA       LATBbits.LATB7        // Physical Pin 28 (SDA)
#define I2C_SCL_IN    PORTBbits.RB6
#define I2C_SDA_IN    PORTBbits.RB7
#define WDT_ON()      (WDTCON0bits.SEN = 1)
#define WDT_OFF()     (WDTCON0bits.SEN = 0)
#else
#define I2C_SCL       TRISBbits.TRISB6   // 1 = Float High, 0 = Drive Low
#define I2C_SCL_IN    PORTBbits.RB6
#define I2C_SDA       TRISBbits.TRISB7   // 1 = Float High, 0 = Drive Low
#define I2C_SDA_IN    PORTBbits.RB7
#define WDT_ON()      (WDTCONbits.SWDTEN = 1)
#define WDT_OFF()     (WDTCONbits.SWDTEN = 0)
#endif

// ADC channels
#define ADC_CH_FWD    1                    // forward detector, RA1
#define ADC_CH_REV    0                    // reverse detector, RA0

enum { REF_FVR1, REF_FVR2, REF_VDD };

void hal_init(void);
void delay_ms(uint16_t ms);
void meas_init(void);
void adc_ref(uint8_t ref);
uint8_t nvm_read(uint8_t a);
void nvm_write(uint8_t a, uint8_t v);

// one conversion (a macro: saves a hardware stack level in the tuning)
#ifdef __16F18857
#define ADC_READ(ch, res) do { \
   ADPCH = (ch); \
   __delay_us(20);               /* acquisition */ \
   ADCON0bits.ADGO = 1; \
   while(ADCON0bits.ADGO) continue; \
   (res) = (uint16_t)ADRESH << 8 | ADRESL; \
} while(0)
#else
#define ADC_READ(ch, res) do { \
   ADCON0bits.CHS = (ch); \
   __delay_us(20);               /* acquisition */ \
   ADCON0bits.ADGO = 1; \
   while(ADCON0bits.ADGO) continue; \
   (res) = (uint16_t)ADRESH << 8 | ADRESL; \
} while(0)
#endif

#endif

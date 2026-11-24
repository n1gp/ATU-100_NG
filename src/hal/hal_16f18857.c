#include "hal.h"
#include "nvm.h"

#pragma config FEXTOSC = OFF, RSTOSC = HFINT32, CLKOUTEN = OFF, CSWEN = ON, FCMEN = ON
#pragma config MCLRE = ON, PWRTE = ON, LPBOREN = OFF, BOREN = NSLEEP, BORV = HI
#pragma config ZCD = OFF, PPS1WAY = ON, STVREN = ON, DEBUG = OFF
#pragma config WDTCPS = WDTCPS_13, WDTE = SWDTEN, WDTCWS = WDTCWS_7, WDTCCS = LFINTOSC
#pragma config WRT = OFF, SCANE = available, LVP = OFF
#pragma config CP = OFF, CPD = OFF

extern uint8_t ref_now;

void hal_init(void) {
   OSCCON1 = 0x60; // HFINTOSC with 1:1 divider
   OSCFRQ  = 0x06; // HFFRQ set explicitly to 32 MHz

   // ----------------------------------------------------------------
   // ANALOG SELECTS (ANSEL)
   // ----------------------------------------------------------------
   // ATU-100 uses RA0 (AN0/RVS) and RA1 (AN1/FWD) for SWR ADC sampling
   ANSELA = 0b00000011; // RA0 and RA1 are Analog, rest digital
   ANSELB = 0b00000000; // All PORTB digital (frees up RB0/RB1 buttons)
   ANSELC = 0b00000000; // All PORTC digital

   // Turn off comparators
   CM1CON0bits.C1ON = 0;
   CM2CON0bits.C2ON = 0;

   // ----------------------------------------------------------------
   // DATA LATCHES (LAT) - Set starting states
   // ----------------------------------------------------------------
   // I2C Bit-Bang pins (RB6/RB7) must idle HIGH (1) 
   LATA = 0b00000000;
   LATB = 0b11000000;   // All PORTB relays off (0)
   LATC = 0b00000000;   // All PORTC relays off (0)

   // ----------------------------------------------------------------
   // PIN DIRECTIONS (TRIS) - 1 = Input, 0 = Output
   // ----------------------------------------------------------------
   // PORTA: RA0, RA1 (ADC inputs).
   TRISA = 0b00000011;
   
   // PORTB: RB0-TUNE-input RB1-AUTO-input RB2-BYP-input, RB6, RB7 (I2C initial outputs/floating outputs)
   TRISB = 0b00000111;
   
   // PORTC: All pins are outputs driving relays (C0-C5, C7, L6, L7)
   TRISC = 0b00000000;

   // ----------------------------------------------------------------
   // OPEN-DRAIN CONFIGURATION FOR I2C
   // ----------------------------------------------------------------
   ODCONBbits.ODCB6 = 1;   // SCL pin open-drain
   ODCONBbits.ODCB7 = 1;   // SDA pin open-drain

   WPUBbits.WPUB1 = 1; // need pullups on RB1 and RB2
   WPUBbits.WPUB2 = 1;

   // ----------------------------------------------------------------
   // PERIPHERAL MODULE DISABLE (PMD) - Kept stock from ATU-10_NG
   // ----------------------------------------------------------------
   // Unused modules off. Kept on: NVM (data EEPROM), FVR, IOC (wake-up by
   // the button), Timer0, ADC.
   PMD0 = 0b00011010;   // CLKR, SCAN, CRC off
   PMD1 = 0b11111110;   // all timers but Timer0 off
   PMD2 = 0b01000111;   // ZCD, comparators, DAC off
   PMD3 = 0b01111111;   // CCP, PWM off
   PMD4 = 0b01010111;   // CWG, MSSP, UART off
   PMD5 = 0b11011111;   // DSM, CLC, SMT off

   // ----------------------------------------------------------------
   // TIMER0 CONFIGURATION (1ms Heartbeat Interrupt @ 32MHz Fosc)
   // ----------------------------------------------------------------
   // Timer0: 8 bit with period register, Fosc/4 / 32 = 250 kHz, 250 counts:
   // an interrupt every 1 ms exactly, no reloading
   T0CON1 = 0b01000101;
   T0CON0 = 0b00000000;
   TMR0H = 249;
   TMR0L = 0;
   PIR0bits.TMR0IF = 0;
   PIE0bits.TMR0IE = 1;
   T0CON0bits.T0EN = 1;
   INTCONbits.GIE = 1;
}

void delay_ms(uint16_t ms) {
   while(ms--) {
      CLRWDT();
      __delay_ms(1);
   }
}

void meas_init(void) {
   ADCON0 = 0;
   ADCON1 = 0;
   ADCON2 = 0;                   // Basic mode configuration
   ADCON3 = 0;
   ADACQ = 0;                    // Acquisition time is handled manually in software
   ADPRE = 0;
   ADCAP = 0;

   // ----------------------------------------------------------------
   // PIC16F18857 SPECIFIC ADC CLOCK & FORMAT CONFIGURATION
   // ----------------------------------------------------------------
   // Set result formatting to Right-Justified
   ADCON0bits.ADFRM = 1;         // 1 = Right-justified result formatting
   
   // Configure the ADC Clock Source (Set to FRC Dedicated Internal RC oscillator)
   ADCLK = 0x3F;                 // Sets the CS (Clock Select) bit to internal FRC
   
   // Power on the ADC module
   ADCON0bits.ADON = 1;          // Enable the ADC module system
   
   ref_now = 0xFF;
}

void adc_ref(uint8_t ref) {
   if(ref == ref_now) return;
   ref_now = ref;
   if(ref == REF_VDD) ADREF = 0b00000000;
   else {
      FVRCONbits.ADFVR = ref == REF_FVR1 ? 0b01 : 0b10;
      FVRCONbits.FVREN = 1;
      while(!FVRCONbits.FVRRDY) continue;
      ADREF = 0b00000011;        // Vref+ = FVR
   }
   __delay_us(100);              // reference settles
}

uint8_t nvm_read(uint8_t a) {
   while(NVMCON1bits.WR) continue;
   NVMCON1bits.NVMREGS = 1;    // data EEPROM at 0xF000
   NVMADRH = 0x70;
   NVMADRL = a;
   NVMCON1bits.RD = 1;
   return NVMDATL;
}

void nvm_write(uint8_t a, uint8_t v) {
   uint8_t gie;
   if(nvm_read(a) == v) return;
   NVMCON1bits.NVMREGS = 1;
   NVMADRH = 0x70;
   NVMADRL = a;
   NVMDATL = v;
   gie = INTCONbits.GIE;
   INTCONbits.GIE = 0;
   NVMCON1bits.WREN = 1;
   NVMCON2 = 0x55;             // unlock sequence
   NVMCON2 = 0xAA;
   NVMCON1bits.WR = 1;
   INTCONbits.GIE = gie;
   while(NVMCON1bits.WR) CLRWDT();
   NVMCON1bits.WREN = 0;
}

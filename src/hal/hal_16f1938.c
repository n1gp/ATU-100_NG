#include "hal.h"

// CONFIG1
#pragma config FOSC = INTOSC    // Internal Oscillator (I/O function on CLKIN/CLKOUT)
#pragma config WDTE = ON        // Watchdog Timer Enabled
#pragma config PWRTE = ON       // Power-up Timer Enabled (gives 5V rail & OLED 64ms to stabilize on boot)
#pragma config MCLRE = ON       // MCLR/VPP pin function is MCLR
#pragma config CP = OFF         // Program Memory Code Protection Disabled
#pragma config CPD = OFF        // Data Memory Code Protection Disabled
#pragma config BOREN = ON       // Brown-out Reset Enabled
#pragma config CLKOUTEN = OFF   // Clock Out Disabled (frees RA6 as normal I/O)
#pragma config IESO = OFF       // Internal/External Switchover Disabled
#pragma config FCMEN = OFF      // Fail-Safe Clock Monitor Disabled

// CONFIG2
#pragma config WRT = OFF        // Flash Memory Self-Write Protection Disabled
#pragma config VCAPEN = OFF     // Voltage Regulator Capacitor Disabled (frees RA0/RA5/RA6)
#pragma config PLLEN = ON       // 4x PLL Enabled (8 MHz INTOSC x 4 = 32 MHz)
#pragma config STVREN = ON      // Stack Overflow/Underflow Reset Enabled
#pragma config BORV = LO        // Brown-out Reset Voltage set to low trip point (1.9V)
#pragma config LVP = ON         // Low-Voltage Programming Enabled

extern uint8_t ref_now;

void hal_init(void) {
    CLRWDT();
    WDTCONbits.WDTPS4 = 0;
    WDTCONbits.WDTPS3 = 1;
    WDTCONbits.WDTPS2 = 0; // 1 sec WDT
    WDTCONbits.WDTPS1 = 1;
    WDTCONbits.WDTPS0 = 0;
    CLRWDT();


    ANSELAbits.ANSELA = 0;
    ANSELAbits.ANSA0 = 1; // analog inputs
    ANSELAbits.ANSA1 = 1;
    ANSELBbits.ANSELB = 0; // all as digital

   // Turn off comparators
   CM1CON0bits.C1ON = 0;
   CM2CON0bits.C2ON = 0;

    // Enable 32MHz Clock
    // Replace OSCCON1 and OSCFRQ with:
    //OSCCON = 0b11110000; // SPLLEN = 1 (4x PLL), IRCF = 1110 (8 MHz -> 32 MHz), SCS = 00

    OSCCON = 0b01110000;
    while(!OSCSTATbits.PLLR);   // Wait for PLL to be ready/locked

   // ----------------------------------------------------------------
   // DATA LATCHES (LAT) - Set starting states
   // ----------------------------------------------------------------
   LATA = 0b00000000;
   LATB = 0b00000000;   // All PORTB relays off (0)
   LATC = 0b00000000;   // All PORTC relays off (0)

   // ----------------------------------------------------------------
   // PIN DIRECTIONS (TRIS) - 1 = Input, 0 = Output
   // ----------------------------------------------------------------
   // PORTA: RA0, RA1 (ADC inputs).
   TRISA = 0b00000011;
   
   // PORTB: RB0-TUNE-input RB1-AUTO-input RB2-BYP-input, RB6, RB7 (I2C initial outputs/floating outputs)
   TRISB = 0b11000111;
   
   // PORTC: All pins are outputs driving relays (C0-C5, C7, L6, L7)
   TRISC = 0b00000000;

    ADCON1bits.ADPREF0 = 1; // ADC with the internal reference
    ADCON1bits.ADPREF1 = 1;

    /* init the ADC here */
    FVRCON = 0x81; /* enable FVR */
    ADCON1 = 0xA3;  /* use Fosc/32 */
    ADCON0bits.ADON = 1;

   // ----------------------------------------------------------------
   // TIMER0 CONFIGURATION (1ms Heartbeat Interrupt @ 32MHz Fosc)
   // ----------------------------------------------------------------
   OPTION_REGbits.TMR0CS = 0; // Clock source = Fosc/4 (8 MHz @ 32 MHz Fosc)
   OPTION_REGbits.PSA = 0;    // Assign prescaler to Timer0
   OPTION_REGbits.PS = 0b100; // 1:32 prescaler -> 250 kHz tick
   TMR0 = 6;                  // 256 - 250 = 6 -> 1 ms interrupt
   INTCONbits.TMR0IF = 0;
   INTCONbits.TMR0IE = 1;

    OPTION_REGbits.nWPUEN = 0;
    WPUBbits.WPUB0 = 1; // PORTB0 Pull-up resistor
    WPUBbits.WPUB1 = 1; // PORTB1 Pull-up resistor
    WPUBbits.WPUB2 = 1; // PORTB2 Pull-up resistor
    //interrupt setting
    INTCONbits.PEIE = 1;  // Enable Peripheral Interrupts
    INTCONbits.GIE = 1;   // Enable Global Interrupts
}

void delay_ms(uint16_t ms) {
   while(ms--) {
      CLRWDT();
      __delay_ms(1);
   }
}

void meas_init(void) {
   ADCON1bits.ADFM = 1;       // Right-justified result (replaces ADCON0bits.ADFRM)
   ADCON1bits.ADCS = 0b010;   // Fosc/32 conversion clock (1.0 us TAD @ 32 MHz)
   ADCON1bits.ADNREF = 0;     // Vref- = VSS
   ADCON0bits.ADON = 1;       // Enable ADC module

   ref_now = 0xFF;
}

void adc_ref(uint8_t ref) {
    if(ref == ref_now) return;
    ref_now = ref;

    switch(ref) {
        case REF_FVR1:
            FVRCON = 0x81;             // Configure internal FVR to 1x (1.024V)
            while(!FVRCONbits.FVRRDY); // Wait for voltage rail to lock
            ADCON1 = 0xA3;             // Right-Justified, Fosc/32, Ref+ tied to FVR
            break;
            
        case REF_FVR2:
            FVRCON = 0x82;             // Configure internal FVR to 2x (2.048V)
            while(!FVRCONbits.FVRRDY); // Wait for voltage rail to lock
            ADCON1 = 0xA3;             // Right-Justified, Fosc/32, Ref+ tied to FVR
            break;
            
        default: // REF_VDD
            // Safely fall back to the 5V system supply rail for maximum range
            ADCON1 = 0xA0;             // Right-Justified, Fosc/32, Ref+ tied to VDD (5.0V)
            break;
    }
    __delay_us(100);
}

uint8_t nvm_read(uint8_t a) {
   while(EECON1bits.WR) continue;
   EEADRL = a;
   EECON1bits.CFGS = 0;       // Access EEPROM/Flash (not Config)
   EECON1bits.EEPGD = 0;      // Access Data EEPROM (not Program Flash)
   EECON1bits.RD = 1;
   return EEDATL;
}

void nvm_write(uint8_t a, uint8_t v) {
   uint8_t gie;
   if(nvm_read(a) == v) return;
   while (EECON1bits.WR) continue;
   EEADRL = a;
   EEDATL = v;
   EECON1bits.CFGS = 0;
   EECON1bits.EEPGD = 0;
   EECON1bits.WREN = 1;
   gie = INTCONbits.GIE;
   INTCONbits.GIE = 0;
   EECON2 = 0x55;
   EECON2 = 0xAA;
   EECON1bits.WR = 1;
   while (EECON1bits.WR) CLRWDT();
   EECON1bits.WREN = 0;
   INTCONbits.GIE = gie;
}

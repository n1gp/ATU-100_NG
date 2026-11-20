#include "board.h"
#include "uart.h"

#pragma config FOSC = INTOSC    // Oscillator Selection (Internal oscillator)
#pragma config PLLEN = ON

void board_init(void) {
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

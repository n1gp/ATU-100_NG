// Configuration words of the PIC16LF18877.
//
// The tuner is updated by its on-board USB programmer (PIC16F1454), which
// writes the hex file through ICSP in low-voltage mode. To keep that path
// open whatever the firmware does, these bits must never change:
//   LVP = ON, MCLRE = ON  - the programmer holds the PIC in reset via MCLR
//   CP = OFF, CPD = OFF   - no read protection
//   WRT = OFF             - no write protection
// The Makefile checks them in the finished hex file.
//
// Other settings:
// - brown-out reset at 2.7 V while running (off in sleep: no extra current)
// - watchdog of about 8 s, switched on by software after the start and off
//   in sleep, so a hang resets the tuner

#include <xc.h>

// Configuration Bits Setup for PIC16F1983
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

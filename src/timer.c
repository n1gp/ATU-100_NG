#include "hal.h"
#include "timer.h"
#include "uart.h"
#include "icom_interface.h"
#include "cells.h"

static volatile uint16_t tick;
static volatile uint8_t div10;
volatile uint8_t btn_tune_held, btn_tune_released, btn_auto_held, btn_auto_released, btn_byp_held, btn_byp_released;

#ifndef ENABLE_EUSART
volatile uint8_t tx_shift = 0;
volatile uint8_t tx_bits_left = 0;
volatile uint8_t tx_subtick = 0;
static volatile uint8_t rx_state = 0;
static volatile uint8_t rx_shift = 0;
static volatile uint8_t rx_subtick = 0;

extern volatile uint8_t bb_fifo[BB_FIFO_SIZE];
extern volatile uint8_t bb_head;
extern volatile uint8_t bb_tail;
#endif

void __interrupt() isr(void) {
#ifdef __16F18857
    if (PIR4bits.TMR2IF) {
        PIR4bits.TMR2IF = 0;
#else
    if (PIE1bits.TMR2IE && PIR1bits.TMR2IF) {
        PIR1bits.TMR2IF = 0;
#endif

#ifndef ENABLE_EUSART // bitbang instead
        // --- TRANSMIT ENGINE ---
        if (tx_bits_left > 0) {
            if (++tx_subtick >= 4) {
                tx_subtick = 0;
                if (tx_bits_left == 10) {
                    UART_TX_LAT = 0;               // Start bit
                } else if (tx_bits_left > 1) {
                    UART_TX_LAT = (tx_shift & 1);  // Data bit (LSB first)
                    tx_shift >>= 1;
                } else {
                    UART_TX_LAT = 1;               // Stop bit
                }
                tx_bits_left--;
            }
        }

        // --- RECEIVE ENGINE (4x Oversampling with Start-Bit Validation) ---
        if (rx_state == 0) {
            if (UART_RX_PORT == 0) {               // Potential start bit falling edge
                rx_state = 1;
                rx_subtick = 0;
                rx_shift = 0;
            }
        } else if (rx_state == 1) {
            if (++rx_subtick >= 2) {               // Center of start bit (0.50 bit)
                if (UART_RX_PORT == 0) {
                    rx_subtick = 0;
                    rx_state = 2;                  // Valid start bit! Next sample in 4 ticks (1.50 bit)
                } else {
                    rx_state = 0;                  // Noise glitch; re-arm immediately
                }
            }
        } else {
            if (++rx_subtick >= 4) {               // Exact center of bits 0..7 and stop bit
                rx_subtick = 0;
                if (rx_state <= 9) {               // States 2..9 = Data bits 0..7
                    rx_shift >>= 1;
                    if (UART_RX_PORT) {
                        rx_shift |= 0x80;
                    }
                    rx_state++;
                } else {                           // State 10 = Center of Stop bit (9.50 bit)
                    if (UART_RX_PORT == 1) {
                        uint8_t next_head = (bb_head + 1u) & BB_FIFO_MASK;
                        if (next_head != bb_tail) {
                            bb_fifo[bb_head] = rx_shift;
                            bb_head = next_head;
                        }
                    }
                    rx_state = 0;
                }
            }
        }
#endif
    }

#ifdef __16F18857
    if (PIE0bits.TMR0IE && PIR0bits.TMR0IF) {
        PIR0bits.TMR0IF = 0;
#else
    if (INTCONbits.TMR0IE && INTCONbits.TMR0IF) {
        TMR0 += 6;
        INTCONbits.TMR0IF = 0;
#endif
        tick++;

        if(++div10 >= 10) {
           div10 = 0;
           if(cfg[CFG_EXT] == INTERFACE_ICOM) icom_interface_timer_tick();
           if(BUTTON_TUNE_DOWN) {
              if(btn_tune_held != 255) btn_tune_held++;
           }
           else if(btn_tune_held) {
              btn_tune_released = btn_tune_held;
              btn_tune_held = 0;
           }
           if(BUTTON_AUTO_DOWN) {
              if(btn_auto_held != 255) btn_auto_held++;
           }
           else if(btn_auto_held) {
              btn_auto_released = btn_auto_held;
              btn_auto_held = 0;
           }
           if(BUTTON_BYP_DOWN) {
              if(btn_byp_held != 255) btn_byp_held++;
           }
           else if(btn_byp_held) {
              btn_byp_released = btn_byp_held;
              btn_byp_held = 0;
           }
        }
    }
}

uint16_t tick_ms(void) {
   uint16_t t;
   INTCONbits.GIE = 0;
   t = tick;
   INTCONbits.GIE = 1;
   return t;
}

#include <xc.h>
#include <stdint.h>
#include <stdbool.h>
#include "board.h"
#include "uart.h"
#include "icom_interface.h"
#include "timer.h"

static volatile uint16_t tick;   // (or extern if incremented in main.c ISR)
static volatile uint8_t div10;
volatile uint8_t btn_tune_held, btn_tune_released, btn_auto_held, btn_auto_released, btn_byp_held, btn_byp_released;

void __interrupt() isr(void) {
    if (INTCONbits.TMR0IE && INTCONbits.TMR0IF) {
        TMR0 += 6;
        INTCONbits.TMR0IF = 0;
        tick++;

        if (++div10 >= 10) {
            div10 = 0;
            icom_interface_timer_tick();
            if (BUTTON_TUNE_DOWN) {
                if (btn_tune_held != 255) btn_tune_held++;
            } else if (btn_tune_held) {
                btn_tune_released = btn_tune_held;
                btn_tune_held = 0;
            }
            if (BUTTON_AUTO_DOWN) {
                if (btn_auto_held != 255) btn_auto_held++;
            } else if (btn_auto_held) {
                btn_auto_released = btn_auto_held;
                btn_auto_held = 0;
            }
            if (BUTTON_BYP_DOWN) {
                if (btn_byp_held != 255) btn_byp_held++;
            } else if (btn_byp_held) {
                btn_byp_released = btn_byp_held;
                btn_byp_held = 0;
            }
        }
    }

    if (PIE1bits.TMR2IE && PIR1bits.TMR2IF) {
        PIR1bits.TMR2IF = 0;
        bitbang_uart_isr();
    }
}

uint16_t tick_ms(void) {
    uint16_t t;
    uint8_t gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;
    t = tick;
    INTCONbits.GIE = gie;
    return t;
}

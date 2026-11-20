#include "uart.h"

uart_command_result_t uart_command;

static char cmd_buf[8];
static uint8_t cmd_len = 0;

#ifdef ENABLE_EUSART
void UART_init(void) {
    // ----------------------------------------------------------------
    // PIN DIRECTION & DIGITAL SETUP
    // ----------------------------------------------------------------
    TRISCbits.TRISC6 = 0; // RC6 is TX (Output)
    TRISCbits.TRISC7 = 1; // RC7 is RX (Input)
    
    // ----------------------------------------------------------------
    // Formula: SP1BRG = (Fosc / (4 * Baud)) - 1
    // 32,000,000 / (4 * 9600) - 1 = 832.33 -> Use 832 (0x0340)
    // ----------------------------------------------------------------
    BAUDCONbits.BRG16 = 1; // Use 16-bit Baud Rate Generator
    TXSTAbits.BRGH = 1;    // High-speed baud rate calculation mode
    SPBRGL = 68;           // lower 8-bits
    SPBRGH = 0;            // upper 8-bits

    // ----------------------------------------------------------------
    // TRANSMIT/RECEIVE ENGINE CONFIGURATION
    // ----------------------------------------------------------------
    TXSTAbits.SYNC = 0;    // Asynchronous mode operation
    RCSTAbits.SPEN = 1;    // Enable Serial Port pins
    TXSTAbits.TXEN = 1;    // Enable Transmitter circuitry
    RCSTAbits.CREN = 1;    // Enable Continuous Receive circuitry

    // ----------------------------------------------------------------
    // DISABLE HARDWARE INTERRUPTS
    // ----------------------------------------------------------------
    PIR1bits.RCIF = 0;    // Clear the EUSART Receive Interrupt Flag
    PIE1bits.RCIE = 0;    // Disable the EUSART Receive Interrupt
    
    PIE1bits.TXIE = 0;    // Keep TX interrupt disabled initially (turned on when data exists)
    PIR1bits.TXIF = 0;    // Clear the hardware flag
}

uint8_t UART_Read_Char(char *out) {
   if (RCSTAbits.OERR) {
      RCSTAbits.CREN = 0;          // Clear overrun error
      RCSTAbits.CREN = 1;
      cmd_len = 0;                 // Discard partial command
      return 0;
   }
   if (!PIR1bits.RCIF) return 0;   // Nothing waiting in hardware FIFO
   if (RCSTAbits.FERR) {
      volatile uint8_t dummy = RCREG;
      cmd_len = 0;
      return 0;
   }
   *out = (char)RCREG;
   return 1;
}

void UART_Write_Char(uint8_t data) {
    while(!TXSTAbits.TRMT); // Wait until the transmit buffer register shifts empty
    TXREG = data;           // Load data byte into transmission channel
}

#else

#define BB_FIFO_SIZE 8
#define BB_FIFO_MASK (BB_FIFO_SIZE - 1)

// Transmit state
static volatile uint8_t tx_shift = 0;
static volatile uint8_t tx_bits_left = 0;
static volatile uint8_t tx_subtick = 0;

// Receive state
static volatile uint8_t rx_state = 0;
static volatile uint8_t rx_shift = 0;
static volatile uint8_t rx_subtick = 0;

static volatile uint8_t bb_fifo[BB_FIFO_SIZE];
static volatile uint8_t bb_head = 0;
static volatile uint8_t bb_tail = 0;

uint8_t UART_Read_Char(char *ch) {
    if (bb_head == bb_tail) {
        return 0;
    }
    *ch = (char)bb_fifo[bb_tail];
    bb_tail = (bb_tail + 1u) & BB_FIFO_MASK;
    return 1;
}

void UART_Write_Char(uint8_t ch) {
    while (tx_bits_left > 0);
    tx_shift = ch;
    tx_subtick = 0;
    tx_bits_left = 10;
}

#define UART_TX_LAT   LATAbits.LATA6
#define UART_TX_TRIS  TRISAbits.TRISA6
#define UART_RX_PORT  PORTAbits.RA7
#define UART_RX_TRIS  TRISAbits.TRISA7

void UART_init(void) {
    // Configure pins (Ensure FOSC = INTOSC and CLKOUTEN = OFF in #pragma config)
    UART_TX_LAT = 1;           // Idle HIGH
    UART_TX_TRIS = 0;          // RA6 output
    UART_RX_TRIS = 1;          // RA7 input

    // Timer2 @ 32 MHz Fosc -> Fosc/4 = 8 MHz
    // Prescaler 1:4 -> 2 MHz clock; PR2 = 138 -> 14.388 kHz (3x 4800 baud)
    T2CON = 0b00000001;        // Prescaler 1:4, Postscaler 1:1, Timer2 OFF
    PR2 = 138;
    TMR2 = 0;
    PIR1bits.TMR2IF = 0;
    PIE1bits.TMR2IE = 1;
    INTCONbits.PEIE = 1;
    T2CONbits.TMR2ON = 1;
}

void bitbang_uart_isr(void) {
        // --- TRANSMIT ENGINE ---
        if (tx_bits_left > 0) {
            if (++tx_subtick >= 3) {
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

        // --- RECEIVE ENGINE (Center-Aligned 3x Oversampling) ---
        if (rx_state == 0) {
            if (UART_RX_PORT == 0) {               // Start bit falling edge
                rx_state = 1;
                rx_subtick = 0;
                rx_shift = 0;
            }
        } else {
            rx_subtick++;
            if ((rx_state == 1 && rx_subtick == 4) ||
                (rx_state >= 2 && rx_state <= 8 && rx_subtick == 3) ||
                (rx_state == 9 && rx_subtick == 2)) {

                rx_subtick = 0;
                if (rx_state <= 8) {
                    rx_shift >>= 1;
                    if (UART_RX_PORT) {
                        rx_shift |= 0x80;
                    }
                    rx_state++;
                } else {
                    // Sampled early-center of Stop Bit (9.17 bit times)
                    if (UART_RX_PORT == 1) {
                        uint8_t next_head = (bb_head + 1u) & BB_FIFO_MASK;
                        if (next_head != bb_tail) {
                            bb_fifo[bb_head] = rx_shift;
                            bb_head = next_head;
                        }
                    }
                    rx_state = 0;                  // Re-armed early for next start bit
                }
            }
        }
}

#endif

void UART_suspend(void) {
    uint8_t gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;

    // Stop Bit-Bang UART (Timer2)
    T2CONbits.TMR2ON = 0;
    PIE1bits.TMR2IE = 0;
    PIR1bits.TMR2IF = 0;

    // Stop Hardware EUSART
    RCSTAbits.SPEN = 0;
    TXSTAbits.TXEN = 0;
    RCSTAbits.CREN = 0;

    INTCONbits.GIE = gie;
}

static uint32_t uart_encode_command_buf(const char *buf, uint8_t length) {
   uint32_t encoded = 0;
   uint8_t i;

   for(i = 0; i < length; i++) {
      uint8_t c = (uint8_t)buf[i];
      if(c >= 'a' && c <= 'z') c -= 'a' - 'A';
      encoded = (encoded << 8) | c;
   }
   for(i = length; i < 4; i++) encoded <<= 8;
   return encoded;
}

uart_command_result_t uart_command_poll(void) {
   uart_command_result_t result = {UART_COMMAND_NONE, 0};
   char ch;

   // Drain any bytes currently sitting in the 2-byte hardware FIFO
   while (UART_Read_Char(&ch)) {
      if (ch == '\r' || ch == '\n' || ch == ' ') continue; // Ignore whitespace

      if (ch == ';') {
         uint8_t name_end = cmd_len;
         uint16_t arg = 0;
         uint8_t i;

         // Look for '=' argument separator
         for (i = 0; i < cmd_len; i++) {
            if (cmd_buf[i] == '=') {
               name_end = i;
               break;
            }
         }

         // Parse integer argument if '=' was found
         if (name_end < cmd_len) {
            uint8_t j = (uint8_t)(name_end + 1u);
            if (j >= cmd_len) { cmd_len = 0; return result; } // Empty arg after '='
            for (; j < cmd_len; j++) {
               if (cmd_buf[j] < '0' || cmd_buf[j] > '9') {
                  cmd_len = 0;
                  return result;
               }
               arg = (uint16_t)(arg * 10u + (uint8_t)(cmd_buf[j] - '0'));
               if (arg > 127u) { cmd_len = 0; return result; }
            }
         }

         // Encode command name (up to 4 characters) using your existing encoder
         if (name_end > 0 && name_end <= 4) {
            result.command = uart_encode_command_buf(cmd_buf, name_end);
            result.argument = (uint8_t)arg;
            if (result.command == UART_COMMAND_NONE) result.argument = 0;
         }

         cmd_len = 0; // Reset buffer for next command
         return result;
      }

      // Accumulate character if buffer isn't full
      if (cmd_len < sizeof(cmd_buf)) {
         cmd_buf[cmd_len++] = ch;
      } else {
         cmd_len = 0; // Overflow: discard garbage and wait for next ';'
      }
   }

   return result;
}

#include "uart.h"
#include "timer.h"

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

#ifdef __16F18857    
    PMD4bits.UART1MD = 0;  // 0 = Module is enabled (turns UART back on)
    ANSELCbits.ANSC6 = 0; // Set RC6 to digital mode
    ANSELCbits.ANSC7 = 0; // Set RC7 to digital mode

    // ----------------------------------------------------------------
    // PPS CONFIGURATION (Map Hardware Engine to Pins 9 & 10)
    // ----------------------------------------------------------------
    PPSLOCK = 0x55; // Unlock sequence step 1
    PPSLOCK = 0xAA; // Unlock sequence step 2
    PPSLOCKbits.PPSLOCKED = 0; // Clear lock bit completely

    // RX Configuration (Input)
    RXPPS = 0x17;         // Connect EUSART RX input to RC7 (0x17 = Port C, Pin 7)
    
    // TX Configuration (Output)
    RC6PPS = 0x10;        // Route RC6 Output multiplexer to TX/CK (0x10 = TX1 output)
    PPSLOCK = 0x55; // ReLock
    PPSLOCK = 0xAA;
    PPSLOCKbits.PPSLOCKED = 1;

    BAUD1CONbits.BRG16 = 1; // Use 16-bit Baud Rate Generator
    TX1STAbits.BRGH = 1;    // High-speed baud rate calculation mode
    SP1BRGL = 68;          // lower 8-bits for 115200 baud (-0.08% error)
    SP1BRGH = 0;           // upper 8-bits

    // ----------------------------------------------------------------
    // TRANSMIT/RECEIVE ENGINE CONFIGURATION
    // ----------------------------------------------------------------
    TX1STAbits.SYNC = 0;    // Asynchronous mode operation
    RC1STAbits.SPEN = 1;    // Enable Serial Port pins
    TX1STAbits.TXEN = 1;    // Enable Transmitter circuitry
    RC1STAbits.CREN = 1;    // Enable Continuous Receive circuitry

    // ----------------------------------------------------------------
    // ENABLE HARDWARE INTERRUPTS
    // ----------------------------------------------------------------
    PIR3bits.RCIF = 0;    // Clear the EUSART Receive Interrupt Flag
    //PIE3bits.RCIE = 1;    // Enable the EUSART Receive Interrupt
    PIE3bits.RCIE = 0;    // Keep disabled when polling PIR3bits.RCIF in UART_Read_Char()

    
    INTCONbits.PEIE = 1;  // Enable Peripheral Interrupts
    INTCONbits.GIE = 1;   // Enable Global Interrupts

    PIE3bits.TXIE = 0;    // Keep TX interrupt disabled initially (turned on when data exists)
    PIR3bits.TXIF = 0;    // Clear the hardware flag
#else
    TXSTAbits.SYNC = 0;    // Asynchronous mode operation
    RCSTAbits.SPEN = 1;    // Enable Serial Port pins
    TXSTAbits.TXEN = 1;    // Enable Transmitter circuitry
    RCSTAbits.CREN = 1;    // Enable Continuous Receive circuitry

    PIR1bits.RCIF = 0;    // Clear the EUSART Receive Interrupt Flag
    PIE1bits.RCIE = 0;    // Disable the EUSART Receive Interrupt
    
    PIE1bits.TXIE = 0;    // Keep TX interrupt disabled initially (turned on when data exists)
    PIR1bits.TXIF = 0;    // Clear the hardware flag

    BAUDCONbits.BRG16 = 1; // Use 16-bit Baud Rate Generator
    TXSTAbits.BRGH = 1;    // High-speed baud rate calculation mode
    SPBRGL = 68;           // lower 8-bits
    SPBRGH = 0;            // upper 8-bits
#endif
}

uint8_t UART_Read_Char(char *out) {
   if (RCSTAbits.OERR) {
      RCSTAbits.CREN = 0;          // Clear overrun error
      RCSTAbits.CREN = 1;
      cmd_len = 0;                 // Discard partial command
      return 0;
   }
#ifdef __16F18857    
   if (!PIR3bits.RCIF) return 0;   // Nothing waiting in hardware FIFO
#else
   if (!PIR1bits.RCIF) return 0;   // Nothing waiting in hardware FIFO
#endif

   if (RCSTAbits.FERR) {
      volatile uint8_t dummy = RCREG;
      (void)dummy;
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

#else //bitbang

extern volatile uint8_t tx_shift;
extern volatile uint8_t tx_bits_left;
extern volatile uint8_t tx_subtick;

volatile uint8_t bb_fifo[BB_FIFO_SIZE];
volatile uint8_t bb_head = 0;
volatile uint8_t bb_tail = 0;

void UART_init(void) {
    UART_TX_LAT = 1;           // Idle HIGH
    UART_TX_TRIS = 0;          // RA6 output
    UART_RX_TRIS = 1;          // RA7 input
#ifdef __16F18857    
    RC6PPS = 0x00;            // Route LATC6 to RC6 (override any EUSART PPS mapping)
    RC1STAbits.SPEN = 0;      // Ensure EUSART is disabled so it doesn't hold the pin
    PMD1bits.TMR2MD = 0;      // Enable Timer2 clock/module (clear bit 2 of PMD1)
    T2CLKCON = 0x01;          // Select Fosc/4 as Timer2 clock source
    T2CON = 0b00100000;       // Prescaler 1:4 (bits 6:4), Postscaler 1:1 (bits 3:0), Timer2 OFF
    T2PR = 103;
    T2TMR = 0;
    PIR4bits.TMR2IF = 0;
    PIE4bits.TMR2IE = 1;
#else
    T2CON = 0b00000001;        // Prescaler 1:4, Postscaler 1:1, Timer2 OFF
    PR2 = 103;
    TMR2 = 0;
    PIR1bits.TMR2IF = 0;
    PIE1bits.TMR2IE = 1;
#endif
    INTCONbits.PEIE = 1;
    T2CONbits.TMR2ON = 1;
    INTCONbits.GIE = 1;
}

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
    uint8_t gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;
    tx_shift = ch;
    tx_subtick = 0;
    tx_bits_left = 10;
    INTCONbits.GIE = gie;
}

#endif

void UART_suspend(void) {
    uint8_t gie = INTCONbits.GIE;
    INTCONbits.GIE = 0;

    // Stop Bit-Bang UART (Timer2)
    T2CONbits.TMR2ON = 0;
#ifdef __16F18857    
    PIR4bits.TMR2IF = 0;
    PIE4bits.TMR2IE = 0;
#else
    PIE1bits.TMR2IE = 0;
    PIR1bits.TMR2IF = 0;
#endif

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

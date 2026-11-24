#ifndef UART_H
#define UART_H

#include "hal.h"

typedef uint32_t uart_command_t;

typedef struct {
   uart_command_t command;
   uint8_t argument;
} uart_command_result_t;

extern uart_command_result_t uart_command;

// Enables a 115200 baud uart, pins C6 output, C7 input.
// Must lift input leg on mosfets Q6 and Q8 and wire
// A6 and A7 to that leg. Then wire C6, C7 and gnd to
// a serial jack.
// Otherwise a bitbang uart is implemented using A6 and A7
// at 4800 baud.
//#define ENABLE_EUSART 1

#ifndef ENABLE_EUSART
#define UART_TX_LAT   LATAbits.LATA6
#define UART_TX_TRIS  TRISAbits.TRISA6
#define UART_RX_PORT  PORTAbits.RA7
#define UART_RX_TRIS  TRISAbits.TRISA7

#define BB_FIFO_SIZE 8
#define BB_FIFO_MASK (BB_FIFO_SIZE - 1)
extern volatile uint8_t bb_fifo[BB_FIFO_SIZE];
extern volatile uint8_t bb_head;
extern volatile uint8_t bb_tail;
#endif

void UART_init(void);
void UART_suspend(void);
uint8_t UART_Read_Char(char *ch);
void UART_Write_Char(uint8_t ch);
uart_command_result_t uart_command_poll(void);

#define UART_COMMAND_CODE(a, b, c, d) \
   (((uint32_t)(uint8_t)(a) << 24) | ((uint32_t)(uint8_t)(b) << 16) | \
    ((uint32_t)(uint8_t)(c) << 8) | (uint32_t)(uint8_t)(d))

#define UART_COMMAND_NONE                 0UL
#define UART_COMMAND_BYPASS_OFF           UART_COMMAND_CODE('B', 'Y', '-', 0)
#define UART_COMMAND_BYPASS_ON            UART_COMMAND_CODE('B', 'Y', '+', 0)
#define UART_COMMAND_AUTO_OFF             UART_COMMAND_CODE('A', 'U', '-', 0)
#define UART_COMMAND_AUTO_ON              UART_COMMAND_CODE('A', 'U', '+', 0)
#define UART_COMMAND_L_SET                UART_COMMAND_CODE('L', 0, 0, 0)
#define UART_COMMAND_C_SET                UART_COMMAND_CODE('C', 0, 0, 0)
#define UART_COMMAND_L_QUERY              UART_COMMAND_CODE('L', '?', 0, 0)
#define UART_COMMAND_C_QUERY              UART_COMMAND_CODE('C', '?', 0, 0)
#define UART_COMMAND_BYPASS_QUERY         UART_COMMAND_CODE('B', 'Y', '?', 0)
#define UART_COMMAND_AUTO_QUERY           UART_COMMAND_CODE('A', 'U', '?', 0)
#define UART_COMMAND_CAP_LOAD             UART_COMMAND_CODE('L', 'C', 0, 0)
#define UART_COMMAND_CAP_SOURCE           UART_COMMAND_CODE('C', 'L', 0, 0)
#define UART_COMMAND_CAP_POSITION_QUERY   UART_COMMAND_CODE('L', 'C', '?', 0)
#define UART_COMMAND_SWR_QUERY             UART_COMMAND_CODE('S', 'W', 'R', '?')
#define UART_COMMAND_LAST_SWR_QUERY        UART_COMMAND_CODE('L', 'S', 'W', '?')
#define UART_COMMAND_PEAK_POWER_QUERY      UART_COMMAND_CODE('P', 'K', '?', 0)
#define UART_COMMAND_POWER_QUERY           UART_COMMAND_CODE('P', 'W', 'R', '?')

// Parse one ';'-terminated command; an unsigned 8-bit decimal may follow '='.
// The argument is zero when omitted; malformed arguments and values over 127
// are rejected. L and C set a 7-bit relay mask (zero when omitted); L? and C?
// query the current mask. Other commands ignore a valid argument. Names are
// case-insensitive; BY? queries bypass, AU? queries auto tune. AU+ and AU-
// enable and disable auto tune.
// LC selects the load side for the shunt capacitor; CL selects the source side;
// LC? queries the side. SWR? queries live SWR in hundredths (0 with no RF);
// LSW? returns the last displayed SWR in hundredths (0 until measured).
// PK? returns peak-held power, and PWR? live power, both in tenths of a watt;
// PWR? returns 0 below the live measurement threshold.

#endif

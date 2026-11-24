#include "hal.h"
#include "relays.h"
#include "cells.h"
#include "meas.h"

relays_t rel;

void relays_set(uint8_t l, uint8_t c, uint8_t sw) {
   // ----------------------------------------------------------------
   // DIRECT INDUCTOR MAPPING (L1 - L7)
   // 1 = Engaged/Selected, 0 = Bypassed
   // ----------------------------------------------------------------
   LATBbits.LATB3 = (l & 0x01) ? 1 : 0;  // L1 (0.05 uH)
   LATAbits.LATA2 = (l & 0x02) ? 1 : 0;  // L2 (0.1 uH)
   LATBbits.LATB4 = (l & 0x04) ? 1 : 0;  // L3 (0.22 uH)
   LATAbits.LATA3 = (l & 0x08) ? 1 : 0;  // L4 (0.45 uH)
   LATBbits.LATB5 = (l & 0x10) ? 1 : 0;  // L5 (1.0 uH)
   LATAbits.LATA5 = (l & 0x20) ? 1 : 0;  // L6 (2.2 uH)
   LATAbits.LATA4 = (l & 0x40) ? 1 : 0;  // L7 (4.5 uH)

   // ----------------------------------------------------------------
   // DIRECT CAPACITOR MAPPING (C1 - C7)
   // 1 = Engaged/Selected, 0 = Bypassed
   // ----------------------------------------------------------------
#ifdef ENABLE_EUSART // you'll need to physically modify the ATU-100
   LATAbits.LATA7 = (c & 0x01) ? 1 : 0;  // C1 (10 pF)
   LATCbits.LATC3 = (c & 0x02) ? 1 : 0;  // C2 (22 pF)
   LATAbits.LATA6 = (c & 0x04) ? 1 : 0;  // C3 (47 pF)
#else
   LATAbits.LATA7 = (c & 0x01) ? 1 : 0;  // C1 (10 pF)
   LATCbits.LATC3 = (c & 0x02) ? 1 : 0;  // C2 (22 pF)
   LATAbits.LATA6 = (c & 0x04) ? 1 : 0;  // C3 (47 pF)
   //RRK change me LATCbits.LATC7 = (c & 0x01) ? 1 : 0;  // C1 (10 pF)
   //RRK change me LATCbits.LATC3 = (c & 0x02) ? 1 : 0;  // C2 (22 pF)
   //RRK change me LATCbits.LATC6 = (c & 0x04) ? 1 : 0;  // C3 (47 pF)
#endif
   LATCbits.LATC2 = (c & 0x08) ? 1 : 0;  // C4 (100 pF)
   LATCbits.LATC5 = (c & 0x10) ? 1 : 0;  // C5 (220 pF)
   LATCbits.LATC1 = (c & 0x20) ? 1 : 0;  // C6 (470 pF)
   LATCbits.LATC4 = (c & 0x40) ? 1 : 0;  // C7 (1000 pF)

   // ----------------------------------------------------------------
   // CAPACITOR SWAP RELAY (TX vs ANT Side)
   // ----------------------------------------------------------------
   LATCbits.LATC0 = sw ? 1 : 0;

   // ----------------------------------------------------------------
   // SETTLING DELAY
   // Give the non-latching relay mechanics a brief moment to finish moving
   // ----------------------------------------------------------------
   delay_ms(cfg[CFG_RELAY_MS]);

   // Update the internal software tracking states
   rel.l = l & 0x7F;
   rel.c = c & 0x7F;
   rel.sw = sw ? 1 : 0;
}

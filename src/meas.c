#include "board.h"
#include "meas.h"

// The detector voltages span from a few mV to the supply voltage, so the
// ADC reference is switched in three ranges:
//   FVR 1.024 V  1 mV per step
//   FVR 2.048 V  2 mV per step
//   Vdd          Vdd / 1024 per step
enum { REF_FVR1, REF_FVR2, REF_VDD };
#define RANGE_TOP 1000           // a sample above this moves to the next range

static uint8_t ref_now = 0xFF;
static uint8_t ovf;

void meas_init(void) {
   ADCON1bits.ADFM = 1;       // Right-justified result (replaces ADCON0bits.ADFRM)
   ADCON1bits.ADCS = 0b010;   // Fosc/32 conversion clock (1.0 us TAD @ 32 MHz)
   ADCON1bits.ADNREF = 0;     // Vref- = VSS
   ADCON0bits.ADON = 1;       // Enable ADC module

   ref_now = 0xFF;
}

static void adc_ref(uint8_t ref) {
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

// one conversion (a macro: saves a hardware stack level in the tuning)
#define ADC_READ(ch, res) do { \
   ADCON0bits.CHS = (ch); \
   __delay_us(20);               /* acquisition */ \
   ADCON0bits.ADGO = 1; \
   while(ADCON0bits.ADGO) continue; \
   (res) = (uint16_t)ADRESH << 8 | ADRESL; \
} while(0)

// Average of n samples of a detector in 1/8 mV. Starts in the most
// sensitive range and moves up when a sample does not fit.
static uint16_t detector(uint8_t ch, uint8_t n) {
   uint8_t ref, i;
   uint16_t s;
   uint32_t sum;
   for(ref = REF_FVR1; ; ref++) {
      adc_ref(ref);
      sum = 0;
      for(i = 0; i < n; i++) {
         ADC_READ(ch, s);
         if(s > RANGE_TOP && ref != REF_VDD) break;
         if(s >= 1023) ovf = 1;
         sum += s;
      }
      if(i == n) break;
   }
   switch(ref) {
      case REF_FVR1: sum *= Q_PER_MV; break;
      case REF_FVR2: sum *= 2 * Q_PER_MV; break;
      default:       sum = (sum * 4915UL * Q_PER_MV + 512) / 1024; break; // Rounded 5V rail
   }

   return (uint16_t)((sum + n / 2) / n);
}

void meas_take(meas_t *m, uint8_t n) {
   uint16_t f1, r1, r2, f2;
   CLRWDT();
   ovf = 0;
   f1 = detector(ADC_CH_FWD, n);
   r1 = detector(ADC_CH_REV, n);
   r2 = detector(ADC_CH_REV, n);
   f2 = detector(ADC_CH_FWD, n);
   meas_finish(m, f1, r1, r2, f2);
   m->overflow = ovf;
}

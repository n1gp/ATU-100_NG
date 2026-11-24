#include "hal.h"
#include "meas.h"

// The detector voltages span from a few mV to the supply voltage, so the
// ADC reference is switched in three ranges:
//   FVR 1.024 V  1 mV per step
//   FVR 2.048 V  2 mV per step
//   Vdd          Vdd / 1024 per step
#define RANGE_TOP 1000           // a sample above this moves to the next range

uint8_t ref_now = 0xFF;
static uint8_t ovf;

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
      // from __16F18857 default:       sum = sum * 4915UL / 1024 * Q_PER_MV; break; // 5v

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

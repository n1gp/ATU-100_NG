// Measuring the forward / reverse detectors with the ADC.

#ifndef MEAS_H
#define MEAS_H

#include <stdint.h>
#include "meas_math.h"

// Full measurement: F1 R1 R2 F2, each the average of n samples (n <= 64)
void meas_take(meas_t *m, uint8_t n);
extern uint8_t ref_now;

#endif

#ifndef __MODULATION_HPP
#define __MODULATION_HPP 1

#include <stdint.h>

#define MODULATION_POTI         A4
#define MODULATION_LED          A5
#define MODULATION_TOLERANCE     5

extern volatile int32_t modulation_value;
extern volatile int32_t modulation_value_new;

#endif
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

uint32_t timer_get_ticks(void);
uint32_t timer_get_frequency(void);
void timer_install(uint32_t frequency_hz);
void timer_wait(uint32_t ticks);
void sleep(uint32_t milliseconds);

#endif

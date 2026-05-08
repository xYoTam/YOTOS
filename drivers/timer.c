#include "timer.h"
#include "irq.h"
#include "io.h"
#include "process.h"

#include <stdint.h>


#define PIT_CHANNEL0_DATA  0x40
#define PIT_COMMAND        0x43
#define PIT_BASE_FREQUENCY 1193180


static volatile uint32_t timer_ticks = 0;
static uint32_t timer_frequency = 0;


uint32_t timer_get_ticks(void) {
    return timer_ticks;
}

uint32_t timer_get_frequency(void) {
    return timer_frequency;
}


static void timer_handler(void) {
    timer_ticks++;
    process_check_kill();
}

static void timer_set_frequency(uint32_t frequency_hz) {
    uint32_t divisor;

    if (frequency_hz == 0) {
        frequency_hz = 100;
    }

    divisor = PIT_BASE_FREQUENCY / frequency_hz;

    if (divisor == 0) {
        divisor = 1;
    }

    timer_frequency = frequency_hz;

    outb(PIT_COMMAND, 0x36);	
    /* 	use channel 0
	send the divisor in two parts: low byte first, then high byte
	use mode 3, square wave generator
	use binary counting */
    outb(PIT_CHANNEL0_DATA, divisor & 0xFF);
    outb(PIT_CHANNEL0_DATA, (divisor >> 8) & 0xFF);
}


void timer_install(uint32_t frequency_hz) {
	timer_ticks = 0;
	timer_set_frequency(frequency_hz);
	irq_install_handler(0, timer_handler);
}


void timer_wait(uint32_t ticks) {
    uint32_t target = timer_get_ticks() + ticks;

    while (timer_get_ticks() < target);
}


void sleep(uint32_t milliseconds) {
    uint32_t ticks;

    ticks = (milliseconds * timer_get_frequency()) / 1000;

    if (ticks == 0 && milliseconds > 0) {
        ticks = 1;
    }

    timer_wait(ticks);
}

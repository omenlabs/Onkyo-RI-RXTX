/*
 * command.c
 *
 * Created: 1/11/2018 9:26:57 PM
 *  Author: John Hickey https://github.com/omenlabs
 */ 
#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <util/delay.h>

#include "command.h"

#define OFF PORTB &= ~(1<<PORTB0)
#define ON PORTB |=  (1<<PORTB0)

void preamble(void) {
	ON;
	_delay_ms(3);
	OFF;
	_delay_ms(1);
}

void mark(void) {
	ON;
	_delay_ms(1);
	OFF;
	_delay_ms(2);
}

void  space(void) {
	ON;
	_delay_ms(1);
	OFF;
	_delay_ms(1);
}

void end(void) {
	ON;
	_delay_ms(1);
	OFF;
	_delay_ms(20);
}

void command(uint16_t command) {
	preamble();
	
	for(int i = 11; i >= 0; i--) {
		if (command & (1 << i)) {
			mark();
		} else {
			space();
		}
	}
	end();
}
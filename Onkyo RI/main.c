/*
 * Onkyo RI.c
 *
 * Created: 1/9/2018 10:33:05 PM
 * Author : John Hickey https://github.com/omenlabs
 */ 

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>
#include <avr/sfr_defs.h>
#include <util/delay.h>
#include <avr/interrupt.h>

#include "uart.h"
#include "command.h"
#include "defines.h"

enum frame_state {READY, RECIEVE, WAIT, SUCCESS, TIMEOUT};
typedef enum frame_state frame_state_t;

typedef struct {
	frame_state_t state;
	uint8_t index;
	uint8_t symbols[13]; // size of each symbol 
	} Frame;
	
Frame myFrame = {.state = READY, .index = 0};

/* These are the known on codes for the Onkyo A-9555 integrated amplifier */
int on_codes[] = {
0x2f, // on plus cd
0x3f, // on plus md
0x7f, // on plus tape
0xe1, // on plus cd
0xe2, // on plus tape
0xe4, // on plus tuner
0xe6, // on plus tuner + mute
0xfb, // on plus tuner
0x17f, // on plus hdd
0x1e0, // on plus md
0x1e1, // on plus LINE!!!
0x1e3, // on plus tuner +mute
0x1e4, // on plus line + mute
0x421, // test mode
0x422, 
0x423,
0x424
};


FILE uart_out = FDEV_SETUP_STREAM(uart_putchar, NULL, _FDEV_SETUP_WRITE);
FILE uart_in  = FDEV_SETUP_STREAM(NULL, uart_getchar, _FDEV_SETUP_READ);

void timer_start() {
	/*
	 We have a 16Mhz CPU.  Setting the prescaler to 1024 will result in a clock of 15625hz.
	 This means that an overflow will occur after ~16ms.  This will provide adequate resolution for the delay after sending.
	 */
	TCNT0 = 0;
	TCCR0B |= (1<<CS02)|(1<<CS00);	
}

void timer_stop() {
	TCCR0B = 0;
	TCNT0 = 0;
}

void recv_init() {
	/* Port D2 (arduino pin 2) */
	DDRD &= ~(1<<PORTD2);  // Set port to input
	
	EICRA |= (1<<ISC01)|(1<<ISC00); // Rising edge
	EIMSK |= (1 << INT0); // Turn on interrupt
}

// Recieve interrupt
ISR (INT0_vect) {
	switch (myFrame.state) {
		case READY:
			timer_start(); // start timer!
			myFrame.state = RECIEVE;
			TIMSK0 |= (1<<TOIE0); // Enable timeout!
			break;
		case RECIEVE:
			myFrame.symbols[myFrame.index] = TCNT0; // Save the counter
			TCNT0=0; // Clear count
			myFrame.index ++;
			if (myFrame.index == 13)
				myFrame.state = WAIT; // Now we wait for the trailer timeout
			break;
	}
}

ISR (TIMER0_OVF_vect)
{
	timer_stop();
	TIMSK0	&= ~(1<<TOIE0); // Turn off overflow interrupt
	myFrame.index = 0;

	if (myFrame.state==WAIT) {
		myFrame.state = SUCCESS;
	} else {
		myFrame.state = TIMEOUT;
	}
}

int frame_ready() {
	if (myFrame.state == SUCCESS) {
		return 1;
	}
	return 0;
}
// Read the frame and then set the status back to READY
uint16_t process_frame() {
	uint16_t payload = 0;
	
	if (!frame_ready()) {
		return 0;
	}
	
	uint8_t timebase = myFrame.symbols[0];
	uint8_t mark_min = (timebase / 4) * 3 - 3;
	uint8_t mark_max = (timebase / 4) * 3 + 3;
	uint8_t space_min = (timebase / 4) * 2 - 3;
	uint8_t space_max = (timebase / 4) * 2 + 3;

	for (int i = 1; i < 13; i++) {
		// something really went wrong if timebase is not the biggest symbol
		if (myFrame.symbols[i] > timebase) {
			payload = 0;
			break;
		}
		
		if ((myFrame.symbols[i]>= mark_min) && (myFrame.symbols[i] <= mark_max)) {
			payload |= 1 << (12 - i);
		} else if ((myFrame.symbols[i]>= space_min) && (myFrame.symbols[i] <= space_max)) {
			// just a sanity check really
			continue;
		} else {
			break;
		}
	}

	myFrame.state = READY;
	return payload;
}

void init(void) {
	DDRB |= _BV(PORTB0);
	uart_init();
	recv_init();
	sei();
}

void on_code_scan() {
	int stop;
	uint16_t cmd;
	cli();
	command(0xea);
	for (uint16_t cmd = 0x0; cmd <= 0xfff; cmd++) {
		stop = 0;            
		for (int j = 0; j < sizeof(on_codes); j++) {
			if (on_codes[j] == cmd) {
				stop = 1;
			}
		}
		if (stop)
		continue;
		
		printf("\nSending command %#0x", cmd);
		
		command(cmd);
		_delay_ms(100);
	}
}

int get_uint(char* prompt, uint16_t *d)
{
	char buf[20];
	
	do {
		printf("%s: ", prompt);

		if (fgets(buf, sizeof buf - 1, stdin) == NULL)
			return 1;
		if (tolower(buf[0]) == 'x')
			return 1;
	} while (sscanf(buf, "%x", d) != 1);
	
	return 0;
}

void scan_mode() {
	uint16_t cmd, start, end;
	char buf[20];
	
	if(get_uint("Start", &start))
		return;
	if(get_uint("End", &end))
		return;

	for (cmd = start; cmd <= end; cmd++) {
		printf("\nSending command %#0x", cmd);
		command(cmd);
		_delay_ms(100);
	}
}

void command_mode() {
	uint16_t cmd;
	char buf[20];
	
	printf("\nCommand mode.  Hit x to exit.");
	for (;;) {
		printf("\nCommand: ");
		if (fgets(buf, sizeof buf - 1, stdin) == NULL)
			break;
		if (tolower(buf[0]) == 'x')
			break;
			
		if(sscanf(buf, "%x", &cmd) == 1) {
			printf("\nSending command %#0x", cmd);
			command(cmd);
		} else {
			printf("\nDidn't grok: %s", buf);
		}
	}
}

recieve_mode() {
	uint16_t cmd = 0;
	
	// enable interrupts
	sei();
	
	for (;;) {
		cmd = process_frame();
		if (cmd != 0) {
			printf("\nCommand: %#0x", cmd); 
		}
	}
}

int main(void)
{
	init();

	char buf[5];
	_delay_ms(100);
	stdout = &uart_out;
	stdin = &uart_in;

    while (1) 
    {
		printf("\n[o]n code scan\n[s]can\n[c]ommand\n[r]ecieve forever\n[o,s,c,r]: ");
		if (fgets(buf, sizeof buf - 1, stdin) == NULL)
			break;
			
		switch(tolower(buf[0])) {
			case 'o':
				on_code_scan();
				break;
			case 'c':
				command_mode();
				break;
			case 's':
				scan_mode();
				break;
			case 'r':
				recieve_mode();
				break;
		}
    }
}


#define F_CPU 8000000UL // Define system clock frequency
#include <avr/io.h>      // Includes ATmega32A specific register definitions
#include <util/delay.h>  // For delay functions

int main(void) {
    DDRC = 0xFF; // Set all pins of Port C as output
    PORTC = 0x00;
    while (1) {
	if( PORTC == 0xFF ) PORTC = 0x00;
        PORTC++;
        _delay_ms(500);
    }
}

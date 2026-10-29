#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

int main(void)
{
    DDRC = 0xFF;    // Port C as output
    PORTC = 0x00;

    DDRD &= ~((1 << PD4) | (1 << PD5));  // PD4, PD5 as inputs

    uint8_t previous = PIND & ((1 << PD4) | (1 << PD5));

    while (1)
    {
        uint8_t current = PIND & ((1 << PD4) | (1 << PD5));

        if (current != previous)
        {
            _delay_ms(20);  // Debounce

            uint8_t confirmed = PIND & ((1 << PD4) | (1 << PD5));

            if (current == confirmed)
            {
                // PD4 pressed (HIGH -> LOW)
                if ((previous & (1 << PD4)) &&
                    !(confirmed & (1 << PD4)))
                {
                    PORTC++;
                }

                // PD5 pressed (HIGH -> LOW)
                if ((previous & (1 << PD5)) &&
                    !(confirmed & (1 << PD5)))
                {
                    PORTC--;
                }

                previous = confirmed;
            }
        }
    }
}

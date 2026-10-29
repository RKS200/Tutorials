/* Generates Fast PWM at ~488Hz */

#include <avr/io.h>

int main(void)
{
    DDRB |= (1 << PB3);  // OC0 output

    OCR0 = 127;          // Approximately 50% duty cycle

    TCCR0 =
        (1 << WGM00) |
        (1 << WGM01) |   // Fast PWM
        (1 << COM01) |   // Non-inverting output
        (1 << CS01)  |
        (1 << CS00);     // Prescaler = 64

    while (1)
    {
    }
}

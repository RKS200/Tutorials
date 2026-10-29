#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* Segment connections */
#define SEG_DP  (1 << PB0)
#define SEG_A   (1 << PB1)
#define SEG_B   (1 << PB2)
#define SEG_C   (1 << PB3)
#define SEG_D   (1 << PB4)
#define SEG_E   (1 << PB5)
#define SEG_F   (1 << PB6)
#define SEG_G   (1 << PB7)

/* Display selection */
#define DISP1   (1 << PA0)
#define DISP2   (1 << PA1)
#define DISP3   (1 << PA2)
#define DISP4   (1 << PA3)

#define ALL_DISPLAYS (DISP1 | DISP2 | DISP3 | DISP4)

/* Common-cathode digit patterns */
const uint8_t digit_pattern[10] =
{
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,             /* 0 */
    SEG_B | SEG_C,                                             /* 1 */
    SEG_A | SEG_B | SEG_D | SEG_E | SEG_G,                     /* 2 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_G,                     /* 3 */
    SEG_B | SEG_C | SEG_F | SEG_G,                             /* 4 */
    SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,                     /* 5 */
    SEG_A | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,             /* 6 */
    SEG_A | SEG_B | SEG_C,                                     /* 7 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,     /* 8 */
    SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G              /* 9 */
};

/* Display one digit */
void display_digit(uint8_t digit, uint8_t display)
{
    /* Disable all displays */
    PORTA &= ~ALL_DISPLAYS;

    /* Output segment pattern */
    PORTB = digit_pattern[digit];

    /* Enable selected display */
    PORTA |= display;

    _delay_ms(2);

    /* Disable selected display */
    PORTA &= ~ALL_DISPLAYS;
}

int main(void)
{
    uint8_t i;

    /* Configure segment pins as outputs */
    DDRB = 0xFF;

    /* Configure PA0-PA3 as outputs */
    DDRA |= ALL_DISPLAYS;

    /* Initially turn off outputs */
    PORTB = 0x00;
    PORTA &= ~ALL_DISPLAYS;

    while (1)
    {
        /* Display 1234 */
        for (i = 0; i < 100; i++)
        {
            display_digit(1, DISP1);
            display_digit(2, DISP2);
            display_digit(3, DISP3);
            display_digit(4, DISP4);
        }
    }
}

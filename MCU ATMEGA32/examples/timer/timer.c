#define F_CPU 8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define MIN_DELAY 100
#define MAX_DELAY 1000
#define STEP      100
#define DEBOUNCE  20

volatile uint32_t overflow_count = 0;

/* Initialize Timer0 */
void ticks_init(void)
{
    TCCR0 = 0;
    TCNT0 = 0;

    /* Clear pending overflow flag */
    TIFR = (1 << TOV0);

    /* Enable Timer0 overflow interrupt */
    TIMSK |= (1 << TOIE0);

    /* Start Timer0 with prescaler 8 */
    TCCR0 = (1 << CS01);

    sei();
}

/* Timer0 overflow ISR */
ISR(TIMER0_OVF_vect)
{
    overflow_count++;
}

/* Return elapsed time in microseconds */
uint32_t get_ticks(void)
{
    return (overflow_count * 256UL) + TCNT0;
}

int main(void)
{
    uint16_t delay_ms = 500;

    uint32_t last_toggle = 0;

    uint8_t pd4_last_read = 1;
    uint8_t pd5_last_read = 1;

    uint8_t pd4_state = 1;
    uint8_t pd5_state = 1;

    uint32_t pd4_change_time = 0;
    uint32_t pd5_change_time = 0;

    /* Configure PORTC as output */
    DDRC = 0xFF;
    PORTC = 0x00;

    /* Configure PD4 and PD5 as inputs */
    DDRD &= ~((1 << PD4) | (1 << PD5));

    /* Initialize timer */
    ticks_init();

    while (1)
    {
        uint32_t now = get_ticks() / 1000UL;

        /* Toggle PORTC at the selected interval */
        if ((uint32_t)(now - last_toggle) >= delay_ms)
        {
            PORTC = ~PORTC;
            last_toggle = now;
        }

        /* Read buttons: active LOW */
        uint8_t pd4_read = (PIND & (1 << PD4)) ? 1 : 0;
        uint8_t pd5_read = (PIND & (1 << PD5)) ? 1 : 0;

        /* Debounce PD4: decrease delay */
        if (pd4_read != pd4_last_read)
        {
            pd4_last_read = pd4_read;
            pd4_change_time = now;
        }

        if ((uint32_t)(now - pd4_change_time) >= DEBOUNCE)
        {
            if (pd4_state != pd4_last_read)
            {
                pd4_state = pd4_last_read;

                /* Decrease once per press */
                if (pd4_state == 0)
                {
                    if (delay_ms > MIN_DELAY)
                    {
                        delay_ms -= STEP;
                    }
                }
            }
        }

        /* Debounce PD5: increase delay */
        if (pd5_read != pd5_last_read)
        {
            pd5_last_read = pd5_read;
            pd5_change_time = now;
        }

        if ((uint32_t)(now - pd5_change_time) >= DEBOUNCE)
        {
            if (pd5_state != pd5_last_read)
            {
                pd5_state = pd5_last_read;

                /* Increase once per press */
                if (pd5_state == 0)
                {
                    if (delay_ms < MAX_DELAY)
                    {
                        delay_ms += STEP;
                    }
                }
            }
        }
    }
}

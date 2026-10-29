#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* Initialize ADC */
void adc_init(void)
{
    /* AVCC reference, ADC channel 0 initially */
    ADMUX = (1 << REFS0);

    /* Enable ADC, prescaler = 64 */
    ADCSRA = (1 << ADEN) |
             (1 << ADPS2) |
             (1 << ADPS1);
}

/* Read ADC channel */
uint16_t adc_read(uint8_t channel)
{
    ADMUX = (ADMUX & 0xE0) | (channel & 0x07);

    ADCSRA |= (1 << ADSC);

    while (ADCSRA & (1 << ADSC))
    {
        /* Wait for conversion */
    }

    return ADC;
}

/* Initialize UART */
void uart_init(void)
{
    /* Baud rate = 9600 at 8 MHz */
    UBRRL = 51;
    UBRRH = 0;

    /* Enable transmitter */
    UCSRB = (1 << TXEN);

    /* Asynchronous UART, 8 data bits, 1 stop bit, no parity */
    UCSRC = (1 << URSEL) |
            (1 << UCSZ1) |
            (1 << UCSZ0);
}

/* Transmit one character */
void uart_transmit(char data)
{
    while (!(UCSRA & (1 << UDRE)))
    {
        /* Wait for transmit buffer */
    }

    UDR = data;
}

/* Transmit a string */
void uart_print(const char *str)
{
    while (*str)
    {
        uart_transmit(*str++);
    }
}

/* Transmit an unsigned integer */
void uart_print_uint(uint16_t value)
{
    char digits[5];
    uint8_t i = 0;

    if (value == 0)
    {
        uart_transmit('0');
        return;
    }

    while (value > 0)
    {
        digits[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0)
    {
        uart_transmit(digits[--i]);
    }
}

int main(void)
{
    uint16_t adc_value;
    uint16_t temperature;

    adc_init();
    uart_init();

    while (1)
    {
        /* Read LM35 on PA5 (ADC5) */
        adc_value = adc_read(5);

        /* Temperature in tenths of a degree Celsius */
        temperature = ((uint32_t)adc_value * 5000UL) / 1023;

        uart_print("Temperature: ");

        /* Print integer part */
        uart_print_uint(temperature / 10);

        uart_transmit('.');

        /* Print decimal part */
        uart_print_uint(temperature % 10);

        uart_print(" C\r\n");

        _delay_ms(1000);
    }
}

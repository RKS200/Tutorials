#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>

/* LCD control pins */
#define LCD_RS  PD4
#define LCD_RW  PD5
#define LCD_E   PD6

/* Generate Enable pulse */
void lcd_enable(void)
{
    PORTD |= (1 << LCD_E);
    _delay_us(1);

    PORTD &= ~(1 << LCD_E);
    _delay_us(100);
}

/* Send command to LCD */
void lcd_cmd(uint8_t cmd)
{
    PORTD &= ~(1 << LCD_RS);   /* RS = 0: Command */
    PORTD &= ~(1 << LCD_RW);   /* R/W = 0: Write */

    PORTB = cmd;

    lcd_enable();

    if (cmd == 0x01 || cmd == 0x02)
    {
        _delay_ms(2);
    }
}

/* Send data to LCD */
void lcd_data(uint8_t data)
{
    PORTD |= (1 << LCD_RS);    /* RS = 1: Data */
    PORTD &= ~(1 << LCD_RW);   /* R/W = 0: Write */

    PORTB = data;

    lcd_enable();
}

/* Initialize LCD */
void lcd_init(void)
{
    DDRB = 0xFF;   /* PORTB as output */

    DDRD |= (1 << LCD_RS) |
            (1 << LCD_RW) |
            (1 << LCD_E);

    _delay_ms(20);

    lcd_cmd(0x38);   /* 8-bit mode, 2 lines, 5x8 font */
    lcd_cmd(0x0C);   /* Display ON, cursor OFF */
    lcd_cmd(0x01);   /* Clear display */
}

/* Print a string */
void lcd_string(const char *str)
{
    while (*str)
    {
        lcd_data(*str);
        str++;
    }
}

int main(void)
{
    lcd_init();
    lcd_string("Hello World");
    lcd_cmd(0xC0);
    lcd_string("Every day is a fresh new beginning.");

    while (1)

    {
    lcd_cmd(0x1F & ~(4));
    _delay_ms(500);
    }
}

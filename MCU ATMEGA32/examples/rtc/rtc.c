#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* Set to 1 to initialize the RTC.
 * Change to 0 after setting the RTC once.
 */
#define SET_RTC_ON_STARTUP  1

/* LCD connections */
#define LCD_RS  PD4
#define LCD_RW  PD5
#define LCD_E   PD6

/* DS1307 I2C address */
#define DS1307_ADDR  0x68

/* ---------------- LCD FUNCTIONS ---------------- */

void lcd_enable(void)
{
    PORTD |= (1 << LCD_E);
    _delay_us(1);

    PORTD &= ~(1 << LCD_E);
    _delay_us(100);
}

void lcd_cmd(uint8_t cmd)
{
    PORTD &= ~(1 << LCD_RS);
    PORTD &= ~(1 << LCD_RW);

    PORTB = cmd;
    lcd_enable();

    if ((cmd == 0x01) || (cmd == 0x02))
    {
        _delay_ms(2);
    }
}

void lcd_data(uint8_t data)
{
    PORTD |= (1 << LCD_RS);
    PORTD &= ~(1 << LCD_RW);

    PORTB = data;
    lcd_enable();
}

void lcd_init(void)
{
    DDRB = 0xFF;

    DDRD |= (1 << LCD_RS) |
            (1 << LCD_RW) |
            (1 << LCD_E);

    _delay_ms(20);

    lcd_cmd(0x38);  /* 8-bit, 2-line mode */
    lcd_cmd(0x0C);  /* Display ON, cursor OFF */
    lcd_cmd(0x01);  /* Clear display */
    lcd_cmd(0x06);  /* Increment cursor */
}

void lcd_string(const char *str)
{
    while (*str != '\0')
    {
        lcd_data(*str);
        str++;
    }
}

void lcd_goto(uint8_t row, uint8_t col)
{
    if (row == 0)
    {
        lcd_cmd(0x80 + col);
    }
    else
    {
        lcd_cmd(0xC0 + col);
    }
}

void lcd_print_int(uint8_t value)
{
    lcd_data('0' + value / 10);
    lcd_data('0' + value % 10);
}

/* ---------------- TWI / I2C FUNCTIONS ---------------- */

void twi_init(void)
{
    /* SCL = 8 MHz / (16 + 2 * 32) = 100 kHz */

    TWSR = 0x00;   /* Prescaler = 1 */
    TWBR = 32;
    TWCR = (1 << TWEN);
}

void twi_start(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWSTA) |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)))
    {
    }
}

void twi_stop(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWSTO) |
           (1 << TWEN);
}

void twi_write(uint8_t data)
{
    TWDR = data;

    TWCR = (1 << TWINT) |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)))
    {
    }
}

uint8_t twi_read_ack(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWEN) |
           (1 << TWEA);

    while (!(TWCR & (1 << TWINT)))
    {
    }

    return TWDR;
}

uint8_t twi_read_nack(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)))
    {
    }

    return TWDR;
}

/* ---------------- DS1307 FUNCTIONS ---------------- */

uint8_t bcd_to_dec(uint8_t value)
{
    return ((value >> 4) * 10) + (value & 0x0F);
}

uint8_t dec_to_bcd(uint8_t value)
{
    return ((value / 10) << 4) | (value % 10);
}

/* Write one byte to a DS1307 register */
void ds1307_write(uint8_t reg, uint8_t data)
{
    twi_start();

    twi_write((DS1307_ADDR << 1) | 0);
    twi_write(reg);
    twi_write(data);

    twi_stop();

    _delay_ms(1);
}

/* Read one byte from a DS1307 register */
uint8_t ds1307_read(uint8_t reg)
{
    uint8_t data;

    twi_start();

    twi_write((DS1307_ADDR << 1) | 0);
    twi_write(reg);

    twi_start();

    twi_write((DS1307_ADDR << 1) | 1);

    data = twi_read_nack();

    twi_stop();

    return data;
}

/* Set default date and time:
 * 20 October 2026, Tuesday, 18:00:00
 */
void ds1307_set_time(void)
{
    twi_start();

    twi_write((DS1307_ADDR << 1) | 0);
    twi_write(0x00);             /* Start at seconds register */

    twi_write(0x00);             /* Seconds: 00 */
    twi_write(0x00);             /* Minutes: 00 */
    twi_write(0x18);             /* Hours: 18 = 6 PM */
    twi_write(0x03);             /* Day: Tuesday */
    twi_write(0x21);             /* Date: 20 */
    twi_write(0x10);             /* Month: October */
    twi_write(0x26);             /* Year: 2026 */

    twi_stop();
}

/* ---------------- MAIN PROGRAM ---------------- */

int main(void)
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t date;
    uint8_t month;
    uint8_t year;

    lcd_init();
    twi_init();

#if SET_RTC_ON_STARTUP
    ds1307_set_time();
#endif

    while (1)
    {
        /* Read time */
        seconds = bcd_to_dec(
            ds1307_read(0x00) & 0x7F);

        minutes = bcd_to_dec(
            ds1307_read(0x01) & 0x7F);

        hours = bcd_to_dec(
            ds1307_read(0x02) & 0x3F);

        /* Read date */
        date = bcd_to_dec(
            ds1307_read(0x04) & 0x3F);

        month = bcd_to_dec(
            ds1307_read(0x05) & 0x1F);

        year = bcd_to_dec(
            ds1307_read(0x06));

        /* Display date */
        lcd_goto(0, 0);

        lcd_print_int(date);
        lcd_data('/');

        lcd_print_int(month);
        lcd_data('/');

        lcd_print_int(year);

        /* Display time in 24-hour format */
        lcd_goto(1, 0);

        lcd_print_int(hours);
        lcd_data(':');

        lcd_print_int(minutes);
        lcd_data(':');

        lcd_print_int(seconds);

        _delay_ms(200);
    }
}

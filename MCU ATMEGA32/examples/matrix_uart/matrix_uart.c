#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* LCD connections */
#define LCD_DATA    PORTB
#define LCD_CTRL    PORTD

#define LCD_RS      PD4
#define LCD_RW      PD5
#define LCD_EN      PD6

/* Keypad scan lines: PC5, PC6, PC7 */
#define SCAN_PORT   PORTC
#define SCAN_DDR    DDRC

/* Keypad read lines: PA4, PA5, PA6, PA7 */
#define READ_PORT   PORTA
#define READ_PIN    PINA
#define READ_DDR    DDRA

/* LCD function declarations */
void lcd_command(uint8_t command);
void lcd_data(uint8_t data);
void lcd_init(void);
void lcd_string(const char *str);
void lcd_print_number(uint8_t number);

/* Keypad function declarations */
void keypad_init(void);
uint8_t keypad_scan(uint8_t *row, uint8_t *col);

/* LCD command */
void lcd_command(uint8_t command)
{
    LCD_DATA = command;

    LCD_CTRL &= ~(1 << LCD_RS);
    LCD_CTRL &= ~(1 << LCD_RW);

    LCD_CTRL |= (1 << LCD_EN);
    _delay_ms(1);
    LCD_CTRL &= ~(1 << LCD_EN);

    _delay_ms(2);
}

/* LCD data */
void lcd_data(uint8_t data)
{
    LCD_DATA = data;

    LCD_CTRL |= (1 << LCD_RS);
    LCD_CTRL &= ~(1 << LCD_RW);

    LCD_CTRL |= (1 << LCD_EN);
    _delay_ms(1);
    LCD_CTRL &= ~(1 << LCD_EN);

    _delay_ms(1);
}

/* Initialize LCD */
void lcd_init(void)
{
    DDRB = 0xFF;

    DDRD |= (1 << LCD_RS) |
            (1 << LCD_RW) |
            (1 << LCD_EN);

    _delay_ms(20);

    lcd_command(0x38);  /* 8-bit, 2-line mode */
    lcd_command(0x0C);  /* Display ON, cursor OFF */
    lcd_command(0x06);  /* Auto-increment cursor */
    lcd_command(0x01);  /* Clear display */
}

/* Print a string */
void lcd_string(const char *str)
{
    while (*str)
    {
        lcd_data((uint8_t)*str);
        str++;
    }
}

/* Print a single-digit number */
void lcd_print_number(uint8_t number)
{
    lcd_data((uint8_t)('0' + number));
}

/* Initialize keypad */
void keypad_init(void)
{
    /* PC5, PC6, PC7 as outputs */
    SCAN_DDR |= (1 << PC5) |
                (1 << PC6) |
                (1 << PC7);

    /* Set all scan lines HIGH initially */
    SCAN_PORT |= (1 << PC5) |
                 (1 << PC6) |
                 (1 << PC7);

    /* PA4, PA5, PA6, PA7 as inputs */
    READ_DDR &= ~((1 << PA4) |
                  (1 << PA5) |
                  (1 << PA6) |
                  (1 << PA7));

    /* Enable internal pull-ups */
    READ_PORT |= (1 << PA4) |
                 (1 << PA5) |
                 (1 << PA6) |
                 (1 << PA7);
}

/* Scan keypad */
uint8_t keypad_scan(uint8_t *row, uint8_t *col)
{
    uint8_t scan;
    uint8_t read;
    uint8_t i;
    uint8_t j;

    /* Scan PC5, PC6, PC7 one at a time */
    for (i = 0; i < 3; i++)
    {
        /* Set all scan lines HIGH */
        SCAN_PORT |= (1 << PC5) |
                     (1 << PC6) |
                     (1 << PC7);

        /* Pull the selected scan line LOW */
        scan = PC5 + i;

        SCAN_PORT &= ~(1 << scan);

        _delay_ms(1);

        /* Read PA4, PA5, PA6, PA7 */
        read = READ_PIN;

        for (j = 0; j < 4; j++)
        {
            if (!(read & (1 << (PA4 + j))))
            {
                *row = i;
                *col = j;

                _delay_ms(20);

                return 1;
            }
        }
    }

    return 0;
}

/* UART initialization: 9600 baud, 8 MHz */
void uart_init(void)
{
    /* Set baud rate */
    UBRRH = 0;
    UBRRL = 51;

    /* Enable transmitter and receiver */
    UCSRB = (1 << RXEN) | (1 << TXEN);

    /* 8-bit data, 1 stop bit, no parity */
    UCSRC = (1 << URSEL) |
            (1 << UCSZ1) |
            (1 << UCSZ0);
}

/* Transmit one character */
void uart_transmit(char data)
{
    while (!(UCSRA & (1 << UDRE)))
    {
    }

    UDR = data;
}

/* Transmit a string */
void uart_string(const char *str)
{
    while (*str)
    {
        uart_transmit(*str);
        str++;
    }
}

/* Transmit a single-digit number */
void uart_print_number(uint8_t number)
{
    uart_transmit((char)('0' + number));
}

/* Main program */
int main(void)
{
    uint8_t row;
    uint8_t col;

    lcd_init();
    keypad_init();
    uart_init();

    lcd_string("Matrix Keypad");
    uart_string("Matrix Keypad Ready\r\n");

    while (1)
    {
        if (keypad_scan(&row, &col))
        {
            /* Display on LCD */
            lcd_command(0xC0);

            lcd_string("(");
            lcd_print_number(row);
            lcd_string(",");
            lcd_print_number(col);
            lcd_string(")   ");

            /* Send through UART */
            uart_string("(");
            uart_print_number(row);
            uart_string(",");
            uart_print_number(col);
            uart_string(")\r\n");
        }
    }
}

#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* EEPROM configuration */
#define EEPROM_SIZE 512
#define EEPROM_ADDR 0x50

/* Button definitions */
#define INC_BUTTON  PD4
#define DEC_BUTTON  PD5
#define LOAD_BUTTON PD6
#define SAVE_BUTTON PD7

#define BUTTON_MASK ((1 << PD4) | (1 << PD5) | \
                     (1 << PD6) | (1 << PD7))

#define COUNTER_MASK 0xF0

/* Initialize TWI for 100 kHz */
void twi_init(void)
{
    TWSR = 0x00;
    TWBR = 32;
    TWCR = (1 << TWEN);
}

/* Generate START condition */
void twi_start(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWSTA) |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)));
}

/* Generate STOP condition */
void twi_stop(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWSTO) |
           (1 << TWEN);
}

/* Transmit one byte */
void twi_write(uint8_t data)
{
    TWDR = data;

    TWCR = (1 << TWINT) |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)));
}

/* Receive one byte and send NACK */
uint8_t twi_read_nack(void)
{
    TWCR = (1 << TWINT) |
           (1 << TWEN);

    while (!(TWCR & (1 << TWINT)));

    return TWDR;
}

/* Write one byte to EEPROM */
void eeprom_write(uint16_t address, uint8_t data)
{
    uint8_t device_address;

    if (address >= EEPROM_SIZE)
        return;

    device_address = EEPROM_ADDR |
                     ((address >> 8) & 0x01);

    twi_start();

    twi_write((device_address << 1) | 0);
    twi_write((uint8_t)address);
    twi_write(data);

    twi_stop();
}

/* Read one byte from EEPROM */
uint8_t eeprom_read(uint16_t address)
{
    uint8_t data;
    uint8_t device_address;

    if (address >= EEPROM_SIZE)
        return 0;

    device_address = EEPROM_ADDR |
                     ((address >> 8) & 0x01);

    /* Set EEPROM memory address */
    twi_start();

    twi_write((device_address << 1) | 0);
    twi_write((uint8_t)address);

    /* Read data */
    twi_start();

    twi_write((device_address << 1) | 1);

    data = twi_read_nack();

    twi_stop();

    return data;
}

/* Get the 4-bit counter */
uint8_t counter_get(void)
{
    return (PORTC >> 4) & 0x0F;
}

/* Update the 4-bit counter */
void counter_set(uint8_t value)
{
    PORTC = (PORTC & 0x0F) |
            ((value & 0x0F) << 4);
}

int main(void)
{
    uint8_t previous;
    uint8_t current;
    uint8_t confirmed;
    uint8_t count;

    /* PC4-PC7 as outputs */
    DDRC |= COUNTER_MASK;

    /* Initialize counter output to zero */
    PORTC &= ~COUNTER_MASK;

    /* PD4-PD7 as inputs with external pull-ups */
    DDRD &= ~BUTTON_MASK;
    PORTD &= ~BUTTON_MASK;

    /* Initialize TWI */
    twi_init();

    previous = PIND & BUTTON_MASK;

    while (1)
    {
        current = PIND & BUTTON_MASK;

        if (current != previous)
        {
            _delay_ms(20);

            confirmed = PIND & BUTTON_MASK;

            if (current == confirmed)
            {
                /* PD4: Increment counter */
                if ((previous & (1 << INC_BUTTON)) &&
                    !(confirmed & (1 << INC_BUTTON)))
                {
                    count = counter_get();
                    count = (count + 1) & 0x0F;

                    counter_set(count);
                }

                /* PD5: Decrement counter */
                if ((previous & (1 << DEC_BUTTON)) &&
                    !(confirmed & (1 << DEC_BUTTON)))
                {
                    count = counter_get();
                    count = (count - 1) & 0x0F;

                    counter_set(count);
                }

                /* PD6: Load counter from EEPROM */
                if ((previous & (1 << LOAD_BUTTON)) &&
                    !(confirmed & (1 << LOAD_BUTTON)))
                {
                    count = eeprom_read(0) & 0x0F;

                    counter_set(count);
                }

                /* PD7: Save counter to EEPROM */
                if ((previous & (1 << SAVE_BUTTON)) &&
                    !(confirmed & (1 << SAVE_BUTTON)))
                {
                    count = counter_get();

                    eeprom_write(0, count);
                }

                previous = confirmed;
            }
        }
    }
}

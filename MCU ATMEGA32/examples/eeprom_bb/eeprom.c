#define F_CPU 8000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* EEPROM configuration */
#define EEPROM_SIZE         512
#define EEPROM_ADDR         0x50

/* I2C pins */
#define SCL_PIN             PC0
#define SDA_PIN             PC1

#define SCL_MASK            (1 << SCL_PIN)
#define SDA_MASK            (1 << SDA_PIN)

/* Button definitions */
#define INC_BUTTON          PD4
#define DEC_BUTTON          PD5
#define LOAD_BUTTON         PD6
#define SAVE_BUTTON         PD7

#define BUTTON_MASK         ((1 << PD4) | (1 << PD5) | \
                             (1 << PD6) | (1 << PD7))

#define COUNTER_MASK        0xF0

/* -------------------------------------------------- */
/* Bit-banged I2C                                      */
/* -------------------------------------------------- */

/* Pull SCL low */
static void i2c_scl_low(void)
{
    PORTC &= ~SCL_MASK;
    DDRC |= SCL_MASK;
}

/* Release SCL */
static void i2c_scl_release(void)
{
    PORTC &= ~SCL_MASK;
    DDRC &= ~SCL_MASK;
}

/* Pull SDA low */
static void i2c_sda_low(void)
{
    PORTC &= ~SDA_MASK;
    DDRC |= SDA_MASK;
}

/* Release SDA */
static void i2c_sda_release(void)
{
    PORTC &= ~SDA_MASK;
    DDRC &= ~SDA_MASK;
}

/* Generate START condition */
static void i2c_start(void)
{
    i2c_sda_release();
    i2c_scl_release();

    _delay_us(5);

    i2c_sda_low();

    _delay_us(5);

    i2c_scl_low();
}

/* Generate STOP condition */
static void i2c_stop(void)
{
    i2c_sda_low();

    _delay_us(5);

    i2c_scl_release();

    _delay_us(5);

    i2c_sda_release();

    _delay_us(5);
}

/* Write one byte and check ACK */
static uint8_t i2c_write_byte(uint8_t data)
{
    uint8_t i;
    uint8_t ack;

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80)
        {
            i2c_sda_release();
        }
        else
        {
            i2c_sda_low();
        }

        _delay_us(5);

        i2c_scl_release();

        _delay_us(5);

        i2c_scl_low();

        data <<= 1;
    }

    /* Release SDA for ACK */
    i2c_sda_release();

    _delay_us(5);

    i2c_scl_release();

    _delay_us(5);

    ack = !(PINC & SDA_MASK);

    i2c_scl_low();

    return ack;
}

/* Read one byte and send ACK or NACK */
static uint8_t i2c_read_byte(uint8_t send_ack)
{
    uint8_t i;
    uint8_t data = 0;

    i2c_sda_release();

    for (i = 0; i < 8; i++)
    {
        data <<= 1;

        i2c_scl_release();

        _delay_us(5);

        if (PINC & SDA_MASK)
        {
            data |= 1;
        }

        i2c_scl_low();

        _delay_us(5);
    }

    /* Send ACK or NACK */
    if (send_ack)
    {
        i2c_sda_low();
    }
    else
    {
        i2c_sda_release();
    }

    _delay_us(5);

    i2c_scl_release();

    _delay_us(5);

    i2c_scl_low();

    i2c_sda_release();

    return data;
}

/* -------------------------------------------------- */
/* EEPROM operations                                  */
/* -------------------------------------------------- */

/* Write one byte to EEPROM */
uint8_t eeprom_write(uint16_t address, uint8_t data)
{
    uint8_t device_address;
    uint8_t success = 1;

    if (address >= EEPROM_SIZE)
    {
        return 0;
    }

    /*
     * Address bit 8 selects the EEPROM block.
     * Assumes the 24C04 uses addresses 0x50 and 0x51.
     */
    device_address = EEPROM_ADDR |
                     ((address >> 8) & 0x01);

    i2c_start();

    if (!i2c_write_byte(device_address << 1))
    {
        success = 0;
    }
    else if (!i2c_write_byte((uint8_t)address))
    {
        success = 0;
    }
    else if (!i2c_write_byte(data))
    {
        success = 0;
    }

    i2c_stop();

    if (success)
    {
        /* Wait for EEPROM internal programming */
        _delay_ms(5);
    }

    return success;
}

/* Read one byte from EEPROM */
uint8_t eeprom_read(uint16_t address, uint8_t *data)
{
    uint8_t device_address;

    if (address >= EEPROM_SIZE)
    {
        return 0;
    }

    device_address = EEPROM_ADDR |
                     ((address >> 8) & 0x01);

    /* Select memory address */
    i2c_start();

    if (!i2c_write_byte(device_address << 1))
    {
        i2c_stop();
        return 0;
    }

    if (!i2c_write_byte((uint8_t)address))
    {
        i2c_stop();
        return 0;
    }

    /* Repeated START */
    i2c_start();

    if (!i2c_write_byte((device_address << 1) | 1))
    {
        i2c_stop();
        return 0;
    }

    /* Read one byte and send NACK */
    *data = i2c_read_byte(0);

    i2c_stop();

    return 1;
}

/* -------------------------------------------------- */
/* Counter operations                                 */
/* -------------------------------------------------- */

static uint8_t counter_get(void)
{
    return (PORTC >> 4) & 0x0F;
}

static void counter_set(uint8_t value)
{
    PORTC = (PORTC & 0x0F) |
            ((value & 0x0F) << 4);
}

/* -------------------------------------------------- */
/* Main                                               */
/* -------------------------------------------------- */

int main(void)
{
    uint8_t previous;
    uint8_t current;
    uint8_t confirmed;
    uint8_t count;

    /* PC4-PC7 as outputs for counter LEDs */
    DDRC |= COUNTER_MASK;

    /* PC0 and PC1 released for bit-banged I2C */
    PORTC &= 0x0F;

    i2c_scl_release();
    i2c_sda_release();

    /* PD4-PD7 as inputs with external pull-ups */
    DDRD &= ~BUTTON_MASK;
    PORTD &= ~BUTTON_MASK;

    /* Initialize counter to zero */
    counter_set(0);

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
                /* PD4: Increment */
                if ((previous & (1 << INC_BUTTON)) &&
                    !(confirmed & (1 << INC_BUTTON)))
                {
                    count = counter_get();
                    count = (count + 1) & 0x0F;

                    counter_set(count);
                }

                /* PD5: Decrement */
                if ((previous & (1 << DEC_BUTTON)) &&
                    !(confirmed & (1 << DEC_BUTTON)))
                {
                    count = counter_get();
                    count = (count - 1) & 0x0F;

                    counter_set(count);
                }

                /* PD6: Load from EEPROM */
                if ((previous & (1 << LOAD_BUTTON)) &&
                    !(confirmed & (1 << LOAD_BUTTON)))
                {
                    if (eeprom_read(0, &count))
                    {
                        counter_set(count);
                    }
                }

                /* PD7: Save to EEPROM */
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

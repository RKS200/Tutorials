# Project Settings
MCU = atmega32a
F_CPU = 8000000UL
TARGET = pwm
SRC = pwm.c

# Toolchain Commands
CC = avr-gcc
OBJCOPY = avr-objcopy
AVRDUDE = avrdude

# Compiler Flags
CFLAGS = -Wall -Os -mmcu=$(MCU) -DF_CPU=$(F_CPU)

all: $(TARGET).hex

$(TARGET).elf: $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET).elf

$(TARGET).hex: $(TARGET).elf
	$(OBJCOPY) -O ihex -R .eeprom $(TARGET).elf $(TARGET).hex

# Optional: Upload to the ATmega32A using a USBasp programmer
flash: $(TARGET).hex
	$(AVRDUDE) -c usbtiny -p m32 -U flash:w:$(TARGET).hex:i

clean:
	rm -f *.o *.elf *.hex


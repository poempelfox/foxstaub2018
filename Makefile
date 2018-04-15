# $Id: Makefile $
# Makefile for Foxstaub2018

CC	= avr-gcc
OBJDUMP	= avr-objdump
OBJCOPY	= avr-objcopy
AVRDUDE	= avrdude
INCDIR	= .
# There are a few additional defines that en- or disable certain features,
# mainly to save space in case you are running out of flash.
# You can add them here.
#  -DCURRENTLYNONE  does nothing
ADDDEFS	= 
# Include support for (virtual) serial console over the USB port?
# Note that this is purely over USB, the microcontrollers serial port is NOT used by
# this - that one is needed for communication with the SDS011 and support for
# that is always compiled in.
# This adds at least 8 KB of bloat.
SERIALCONSOLE = 1

# target mcu (atmega 32u4)
MCU	= atmega32u4
# Since avrdude is generally crappy software (I liked uisp a lot better, too
# bad the project is dead :-/), it cannot use the MCU name everybody else
# uses, it has to invent its own name for it. So this defines the same
# MCU as above, but with the name avrdude understands.
AVRDMCU	= m32u4

# Some more settings
# Clock Frequency of the AVR. Needed for various calculations.
CPUFREQ		= 8000000UL

SRCS	= eeprom.c rfm69.c lufa/console.c main.c
ifeq ($(SERIALCONSOLE), 1)
# The serial console is the only thing needing lufa and adds the whole mess of this dependency.
SRCS	+= lufa/LUFA/Drivers/USB/Core/USBTask.c lufa/LUFA/Drivers/USB/Core/AVR8/Endpoint_AVR8.c lufa/LUFA/Drivers/USB/Core/AVR8/EndpointStream_AVR8.c lufa/LUFA/Drivers/USB/Core/Events.c lufa/LUFA/Drivers/USB/Core/DeviceStandardReq.c lufa/LUFA/Drivers/USB/Core/AVR8/USBController_AVR8.c lufa/LUFA/Drivers/USB/Core/AVR8/USBInterrupt_AVR8.c lufa/Descriptors.c
endif
PROG	= foxstaub2018

# compiler flags
CFLAGS	= -g -Os -Wall -Wno-pointer-sign -std=c99 -mmcu=$(MCU) $(ADDDEFS)
CFLAGS += -DCPUFREQ=$(CPUFREQ) -DF_CPU=$(CPUFREQ)
ifeq ($(SERIALCONSOLE), 1)
CFLAGS +=  -DF_USB=8000000UL -DUSE_LUFA_CONFIG_HEADER -DINTERRUPT_CONTROL_ENDPOINT -DSERIALCONSOLE -I./lufa
endif

# linker flags
LDFLAGS = -g -mmcu=$(MCU) -Wl,-Map,$(PROG).map -Wl,--gc-sections
# For serialconsole to properly print human readable temp/hum values,
# we'll need to add floatingpoint.
# This is about 3 KB of bloat.
ifeq ($(SERIALCONSOLE), 1)
  LDFLAGS += -Wl,-u,vfprintf -lprintf_flt -lm
endif

OBJS	= $(SRCS:.c=.o)

all: compile dump text eeprom
	@echo -n "Compiled size: " && ls -l $(PROG).bin

compile: $(OBJS)
	$(CC) $(LDFLAGS) -o $(PROG).elf $(OBJS)

dump: compile
	$(OBJDUMP) -h -S $(PROG).elf > $(PROG).lst

%o : %c 
	$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

# Create the flash contents
text: compile
	$(OBJCOPY) -j .text -j .data -O ihex $(PROG).elf $(PROG).hex
	$(OBJCOPY) -j .text -j .data -O srec $(PROG).elf $(PROG).srec
	$(OBJCOPY) -j .text -j .data -O binary $(PROG).elf $(PROG).bin

# Rules for building the .eeprom rom images
eeprom: compile
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O ihex $(PROG).elf $(PROG)_eeprom.hex
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O srec $(PROG).elf $(PROG)_eeprom.srec
	$(OBJCOPY) -j .eeprom --change-section-lma .eeprom=0 -O binary $(PROG).elf $(PROG)_eeprom.bin

clean:
	rm -f $(PROG) $(OBJS) *~ lufa/*~ *.elf *.rom *.bin *.eep *.o *.lst *.map *.srec *.hex

fuses:
	@echo "Nothing is known about the fuses yet"

upload: uploadflash
	@echo "Note: 'make upload' does only upload flash, not eeprom, because you manually"
	@echo " need to reset the feather again before upload eeprom is possible."
	@echo "You need to explicitly call 'make uploadeeprom' to upload EEPROM."

uploadflash:
	$(AVRDUDE) -c avr109 -p $(AVRDMCU) -P /dev/ttyACM0 -U flash:w:$(PROG).hex

uploadeeprom:
	$(AVRDUDE) -c avr109 -p $(AVRDMCU) -P /dev/ttyACM0 -U eeprom:w:$(PROG)_eeprom.srec:s


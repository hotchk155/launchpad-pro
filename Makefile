BUILDDIR = build

TOOLS = tools

SOURCES += src/app.c

OBJECTS = $(addprefix $(BUILDDIR)/, $(addsuffix .o, $(basename $(SOURCES))))

INCLUDES += -Iinclude \
-I\

SYX = $(BUILDDIR)/launchpad_pro.syx
LIB = lib/launchpad_pro.a
ELF = $(BUILDDIR)/launchpad_pro.elf
HEX = $(BUILDDIR)/launchpad_pro.hex
HEXTOSYX = $(BUILDDIR)/hextosyx

HOST_CC = g++
CC = arm-none-eabi-gcc
LD = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy

CFLAGS  = -Os -g -Wall -I.\
-D_STM32F103RBT6_  -D_STM3x_  -D_STM32x_ -mthumb -mcpu=cortex-m3 \
-fsigned-char  -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER -DHSE_VALUE=6000000UL \
-DCMSIS -DUSE_GLOBAL_CONFIG   -g3 -ffunction-sections -std=c99  -mlittle-endian \
$(INCLUDES) -o

LDSCRIPT = stm32_flash.ld

LDFLAGS += -T$(LDSCRIPT) -u _start -u _Minimum_Stack_Size  -mcpu=cortex-m3 -mthumb -specs=nano.specs -specs=nosys.specs -nostdlib -Wl,-static  -g3 -N -nostartfiles -Wl,--gc-sections

all: $(SYX)

# build the final sysex file from the ELF
$(SYX): $(HEX) $(HEXTOSYX)
	./$(HEXTOSYX) $(HEX) $(SYX)

# build the tool for conversion of ELF files to sysex ready for upload to the unit
$(HEXTOSYX):
	$(HOST_CC) -Ofast -std=c++0x -I./$(TOOLS)/libintelhex/include ./$(TOOLS)/libintelhex/src/intelhex.cc $(TOOLS)/hextosyx.cpp -o $(HEXTOSYX)

$(HEX): $(ELF)
	$(OBJCOPY) -O ihex $< $@

$(ELF): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(LIB)

$(BUILDDIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

$(BUILDDIR)/%.o: %.s
	mkdir -p $(dir $@)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	rm -rf $(BUILDDIR)
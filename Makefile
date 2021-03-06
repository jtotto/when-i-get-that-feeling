PREFIX = arm-none-eabi-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
AS = $(PREFIX)as
OBJCOPY = $(PREFIX)objcopy
OBJDUMP = $(PREFIX)objdump

CABOOSE = CaboOSe/kernel
PLATFORM = caboose-platform
PRINTF = mini-printf
USPI = uspi/lib
USPIINC = uspi/include

CPPFLAGS = -MMD \
		   -MP \
		   -I. \
		   -I$(CABOOSE) \
		   -I$(PRINTF) \
		   -I$(USPIINC) \
		   -iquote$(CABOOSE)/caboose
CFLAGS = -Wall \
		 -Werror \
		 -std=gnu99 \
		 -march=armv7-a \
		 -mtune=cortex-a7 \
		 -mfloat-abi=hard \
		 -mfpu=vfpv4 \
		 -nostdlib \
		 -nostartfiles \
		 -ffreestanding \
		 -fno-strict-aliasing

CFLAGS += -O3
CFLAGS += -ggdb

ASFLAGS = -mfloat-abi=hard \
		  -mfpu=vfpv4

LDFLAGS = -T caboose-platform/platform.ld
LDLIBS = -lgcc

all: kernel.img

OBJS := $(patsubst %.c, %.o, $(wildcard *.c)) \
	$(patsubst $(CABOOSE)/%.c, $(CABOOSE)/%.o, $(wildcard $(CABOOSE)/*.c)) \
	$(patsubst $(PRINTF)/%.c, $(PRINTF)/%.o, $(wildcard $(PRINTF)/*.c)) \
	$(patsubst $(PLATFORM)/%.c, $(PLATFORM)/%.o, $(wildcard $(PLATFORM)/*.c)) \
	$(patsubst $(USPI)/%.c, $(USPI)/%.o, $(wildcard $(USPI)/*.c))

AOBJS := \
	$(patsubst $(PLATFORM)/%.S, $(PLATFORM)/%.o, $(wildcard $(PLATFORM)/*.S))
$(AOBJS): $(PLATFORM)/offsets.h

OBJS += $(AOBJS)

$(OBJS): Makefile

DEPS = $(OBJS:.o=.d)
-include $(DEPS)

$(PLATFORM)/offsets.h: $(PLATFORM)/offset.hax
	echo "#ifndef OFFSET_HAX_H" > $(PLATFORM)/offsets.h
	echo "#define OFFSET_HAX_H" >> $(PLATFORM)/offsets.h
	cat $^ | $(CC) -S $(CPPFLAGS) $(CFLAGS) -o - -xc - | \
	sed -n 's/HAX :( \([^ ]*\) #/#define \1 /p' >> $(PLATFORM)/offsets.h
	rm -f ./-.d
	echo "#endif" >> $(PLATFORM)/offsets.h

kernel.img: $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o kernel.elf $^ $(LDLIBS)
	$(OBJCOPY) kernel.elf -O binary kernel.img

clean:
	rm -f kernel.elf kernel.img $(DEPS) $(OBJS) $(PLATFORM)/offsets.h

.PHONY: clean offsets.h

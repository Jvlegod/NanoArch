# SDK_ROOT      := /home/jvle/Desktop/works/Embedded/rk3506_linux6.1_sdk_v1.2.0/buildroot/output/rockchip_hd_rk3506g_evm_nand/host/arm-buildroot-linux-gnueabihf/sysroot/
# CROSS_COMPILE ?= /home/jvle/Desktop/works/Embedded/rk3506_linux6.1_sdk_v1.2.0/prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-
CC  := $(CROSS_COMPILE)gcc

CFLAGS  := -Ofast -s --sysroot=$(SDK_ROOT) -DMINIGB_APU_AUDIO_FORMAT_S16SYS
INCLUDES := -I. -I../../ -I$(SDK_ROOT)/usr/include

LDFLAGS := -L$(SDK_ROOT)/usr/lib
LDLIBS  := -lpthread -lasound

TARGET = light-gb
SRCS = light-gb.c fbMiniFB.c minigb_apu/minigb_apu.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
TARGET = light-nes

CC = /home/jvle/Desktop/works/Embedded/rk3506_linux6.1_sdk_v1.2.0/prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-gcc
SYSROOT = /home/jvle/Desktop/works/Embedded/rk3506_linux6.1_sdk_v1.2.0/buildroot/output/rockchip_hd_rk3506g_evm_nand/host/arm-buildroot-linux-gnueabihf/sysroot/

CSRCS = ../input.c \
		../audio.c \
		../config.c

CPPSRCS = ./K6502.cpp \
          ./InfoNES.cpp \
          ./InfoNES_Mapper.cpp \
          ./InfoNES_pAPU.cpp \
          ./InfoNES_System_Linux.cpp

OBJS = $(CSRCS:.c=.o) $(CPPSRCS:.cpp=.o)

CCFLAGS = -O3 -fsigned-char --sysroot=$(SYSROOT)

# ARM specific flags for optimization
# CCFLAGS += -march=armv7-a \
#           -mcpu=cortex-a7 \
#           -mfloat-abi=hard \
#           -mfpu=neon-vfpv4 \
#           -ffast-math

INCLUDES = -I. -I$(SYSROOT)/usr/include

LDFLAGS = --sysroot=$(SYSROOT) \
          -L$(SYSROOT)/usr/lib \
          -lstdc++ -lm -lz -lpthread -lasound

.PHONY: all clean cleanall

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "Linking $(TARGET)..."
	$(CC) -o $@ $(OBJS) $(LDFLAGS)
	@echo "Build successful: $(TARGET)"

%.o: %.cpp
	@echo "Compiling C++ source $<..."
	$(CC) -c $(CCFLAGS) $(INCLUDES) $< -o $@

%.o: %.c
	@echo "Compiling C source $<..."
	$(CC) -c $(CCFLAGS) $(INCLUDES) $< -o $@

clean:
	rm -f $(OBJS)
	@echo "Cleanup objects complete."

cleanall: clean
	rm -f $(TARGET)
	@echo "Cleanup all complete."
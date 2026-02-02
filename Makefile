TARGET = NanoArch
VERSION = 0.1.0

# CROSS_COMPILE ?= /home/jvle/Desktop/works/Embedded/rk3506_linux6.1_sdk_v1.2.0/prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-
# SYSROOT = /home/jvle/Desktop/works/Embedded/rk3506_linux6.1_sdk_v1.2.0/buildroot/output/rockchip_hd_rk3506g_evm_nand/host/arm-buildroot-linux-gnueabihf/sysroot/

CC := $(CROSS_COMPILE)gcc

SDL2_INC = $(SYSROOT)/usr/include/SDL2
SDL2_LIB = $(SYSROOT)/usr/lib

CFLAGS = -Wall -Wextra -std=c11 \
         --sysroot=$(SYSROOT) \
         -I$(SDL2_INC) \
         -D_REENTRANT

LDFLAGS = --sysroot=$(SYSROOT) \
          -L$(SDL2_LIB) \
          -lSDL2 -lSDL2_ttf -lSDL2_image -ldl -lpthread -lm

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

all: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build successful: $(BIN_DIR)/$(TARGET)"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	@echo "Cleanup complete."

run:
	$(BIN_DIR)/$(TARGET)

.PHONY: all clean runTARGET = NanoArch
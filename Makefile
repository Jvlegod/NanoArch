TARGET = NanoArch
CROSS_COMPILE ?= /home/jvle/Desktop/works/Embedded/rk3506_linux6.1_sdk_v1.2.0/prebuilts/gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/arm-none-linux-gnueabihf-
SYSROOT ?= /home/jvle/Desktop/works/Embedded/rk3506_linux6.1_sdk_v1.2.0/buildroot/output/rockchip_hd_rk3506g_evm_nand/host/arm-buildroot-linux-gnueabihf/sysroot/

CC := $(CROSS_COMPILE)gcc

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin
CORE_DIR = cores
GB_DIR  = src/light-gb
NES_DIR = src/light-nes

SDL2_INC = $(SYSROOT)/usr/include/SDL2
SDL2_LIB = $(SYSROOT)/usr/lib

SRCS = $(SRC_DIR)/main.c \
       $(SRC_DIR)/render.c \
       $(SRC_DIR)/config.c \
       $(SRC_DIR)/core_manager.c \
	   $(SRC_DIR)/input.c

OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))

CFLAGS = -Wall -Wextra -std=c11 -O3 \
         --sysroot=$(SYSROOT) \
         -I$(SDL2_INC) \
         -D_REENTRANT

LDFLAGS = --sysroot=$(SYSROOT) \
          -L$(SDL2_LIB) \
          -lSDL2 -lSDL2_ttf -lSDL2_image -lm

.PHONY: all clean run nano gb

all: nano gb nes

nano: $(BIN_DIR)/$(TARGET)

$(BIN_DIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BIN_DIR)
	@echo "Linking $(TARGET)..."
	$(CC) $(OBJS) -o $@ $(LDFLAGS)
	@echo "Build successful: $@"

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

gb:
	@echo "Building Light-GB Core..."
	$(MAKE) -C $(GB_DIR)

nes:
	@echo "Building NES Core..."
	$(MAKE) -C $(NES_DIR)

merge:
	@echo "Merging Cores into Bin Directory..."
	@mkdir -p $(CORE_DIR)
	@cp $(GB_DIR)/light-gb $(CORE_DIR)/
	@cp $(NES_DIR)/light-nes $(CORE_DIR)/
	

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)
	rm -rf $(CORE_DIR)/*
	$(MAKE) -C $(GB_DIR) clean
	$(MAKE) -C $(NES_DIR) cleanall
	@echo "Cleanup complete."

run:
	$(BIN_DIR)/$(TARGET)

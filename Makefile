# Default target if none is specified on the command line
TARGET ?= 16f1938

VERSION := $(subst .,_,$(shell sed -n 's/.*FW_VERSION "\(.*\)"/\1/p' include/version.h))

# Toolchain & DFP base directory (adjust paths to match your Linux install)
CC       := /opt/microchip/xc8/*/bin/xc8-cc
DFP_BASE := $(HOME)/.local/share/microchip/packs

HOST_CC ?= gcc
SIM_SRC  = sim/sim_main.c sim/sim_oled.c src/display.c

# Target-specific configuration
ifeq ($(TARGET),16f1938)
    MCU      := 16F1938
    DFP_PATH := $(DFP_BASE)/PIC12-16F1xxx_DFP/1.8.254/xc8
    HAL_SRC  := src/hal/hal_16f1938.c
    # Example target-specific linker options (e.g., HEF reservation)
    LDFLAGS  := -mrom=default,-3FEF-3FFF
else ifeq ($(TARGET),16f18857)
    MCU      := 16F18857
    DFP_PATH := $(DFP_BASE)/PIC16F1xxxx_DFP/xc8
    HAL_SRC  := src/hal/hal_16f18857.c
    LDFLAGS  :=
else
    $(error Unsupported TARGET '$(TARGET)'. Use TARGET=16f1938 or TARGET=16f18857)
endif

# Directories
BUILD_DIR := build/$(TARGET)
OUT_HEX   := $(BUILD_DIR)/firmware_$(TARGET).hex

# Shared sources + selected HAL source
COMMON_SRCS := src/buttons.c src/cells.c src/display.c src/i2c_soft.c src/icom_interface.c src/main.c src/meas.c src/meas_math.c src/oled.c src/relays.c src/settings.c src/setup.c src/text.c src/timer.c src/tune.c src/uart.c src/nvm.c
SRCS        := $(COMMON_SRCS) $(HAL_SRC)
OBJS        := $(patsubst %.c,$(BUILD_DIR)/%.p1,$(SRCS))

# Compiler flags
CFLAGS := -mcpu=$(MCU) -mdfp="$(DFP_PATH)" -Iinclude -O2 -Wall

# Default rule
all: $(OUT_HEX)

# hardware stack: 16 levels; keep a margin (the interrupt is included)
MAX_STACK = 13

# Link step
$(OUT_HEX): $(OBJS)
	@mkdir -p $(dir $@)
	$(CC) -mcpu=$(MCU) $(CFLAGS) $(LDFLAGS) -Wa,-a -Wl,-Map=$(BUILD_DIR)/firmware_$(TARGET).map -o $@ $^
	cp $(OUT_HEX) build/ATU-100_NG_$(VERSION)_$(TARGET).hex

	@depth=$$(sed -n 's/.*Estimated maximum stack depth \([0-9]*\).*/\1/p' $(BUILD_DIR)/firmware_$(TARGET).lst | sort -n | tail -1); \
	 echo "hardware stack depth incl. interrupt: $$depth of 16 (limit $(MAX_STACK))"; \
	 test -n "$$depth" && test $$depth -le $(MAX_STACK)

# Compile step (mirrors src/ folder structure inside build/$(TARGET)/)
$(BUILD_DIR)/%.p1: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

# Convenience targets
16f1938:
	$(MAKE) TARGET=16f1938

16f18857:
	$(MAKE) TARGET=16f18857

both: 16f1938 16f18857

sim: build/oled_sim
	./build/oled_sim

build/oled_sim: $(SIM_SRC) include/display.h include/oled.h include/font5x8.h
	mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Iinclude -Isrc $(SIM_SRC) -o $@

clean:
	rm -rf build/

.PHONY: all 16f1938 16f18857 both clean


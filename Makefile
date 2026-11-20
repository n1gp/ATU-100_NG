# ATU-100 NG firmware, built with Microchip XC8
#   make          -> build/ATU-100_NG_x_y_z.hex (need to flash it with pickit like tool)
#   make dist     -> dist/: hex file, zip (hex + LICENSE) and LICENSE for a release
#   make clean

# the version comes from src/version.h only
VERSION := $(subst .,_,$(shell sed -n 's/.*FW_VERSION "\(.*\)"/\1/p' src/version.h))
TARGET  = build/ATU-100_NG_$(VERSION)
NAME    = $(notdir $(TARGET))

HOST_CC ?= gcc
SIM_SRC  = sim/sim_main.c sim/sim_oled.c src/display.c

XC8    ?= $(firstword $(wildcard /opt/microchip/xc8/*/bin/xc8-cc) xc8-cc)
CPU     = 16F1938
# Device Family Pack from https://packs.download.microchip.com (XC8 v3+ ships without it)
DFP    ?= $(lastword $(wildcard $(HOME)/.local/share/microchip/packs/PIC12-16F1xxx_DFP/1.8.254/))
SRC     = $(addprefix src/, app.c board.c buttons.c cells.c config.c display.c i2c_soft.c icom_interface.c \
             meas.c meas_math.c nvm.c oled.c relays.c settings.c setup.c text.c timer.c tune.c uart.c)
HDR     = $(wildcard src/*.h)
# hardware stack: 16 levels; keep a margin (the interrupt is included)
MAX_STACK = 13

CFLAGS  = -mcpu=$(CPU) -mdfp=$(DFP)/xc8 -std=c99 -O2 -Isrc

# Reserve the top 32-word flash erase row (0x3FE0 - 0x3FFF) for user configuration
LDFLAGS += -mrom=default,-3FEF-3FFF

all: $(TARGET).hex

build/fw.hex: $(SRC) $(HDR)
	mkdir -p build
	$(XC8) $(CFLAGS) $(LDFLAGS) -Wa,-a -Wl,-Map=build/fw.map -o build/fw.elf $(SRC)
	@depth=$$(sed -n 's/.*Estimated maximum stack depth \([0-9]*\).*/\1/p' build/fw.lst | sort -n | tail -1); \
	 echo "hardware stack depth incl. interrupt: $$depth of 16 (limit $(MAX_STACK))"; \
	 test -n "$$depth" && test $$depth -le $(MAX_STACK)

$(TARGET).hex: build/fw.hex
	cp build/fw.hex $(TARGET).hex

sim: build/oled_sim
	./build/oled_sim

build/oled_sim: $(SIM_SRC) src/display.h src/oled.h src/font5x8.h
	mkdir -p build
	$(HOST_CC) -std=c99 -O2 -Wall -Isrc $(SIM_SRC) -o $@

clean:
	rm -rf build dist

# release files
dist: $(TARGET).hex LICENSE
	rm -rf dist
	mkdir -p dist
	cp $(TARGET).hex LICENSE dist/
	cd dist && zip -q -X $(NAME).zip $(NAME).hex LICENSE

.PHONY: all clean dist

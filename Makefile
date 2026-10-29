
.SUFFIXES:

ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set)
endif

include $(DEVKITPRO)/libnx/switch_rules

TARGET := CuboSimulator

CXX := $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-g++

ARCH := -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE

CXXFLAGS := -O2 -g -Wall -std=gnu++17 \
             -fno-rtti -fno-exceptions $(ARCH) \
             -I$(PORTLIBS)/include \
             -I$(PORTLIBS)/include/SDL2 \
             -I$(LIBNX)/include

LDFLAGS := -specs=$(DEVKITPRO)/libnx/switch.specs \
           $(ARCH) \
           -L$(PORTLIBS)/lib \
           -L$(LIBNX)/lib

LIBS := -lSDL2 -lEGL -lGLESv2 -lglapi -ldrm_nouveau -lnx

.PHONY: all clean

all: $(TARGET).nro

main.o: source/main.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET).elf: main.o
	$(CXX) $(LDFLAGS) $< $(LIBS) -o $@

$(TARGET).nro: $(TARGET).elf
	elf2nro $< $@

clean:
	rm -f main.o $(TARGET).elf $(TARGET).nro

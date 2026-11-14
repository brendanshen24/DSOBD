PSPDEV ?= /Users/brendan/DSCarplay/tools/pspdev
export PATH := $(PSPDEV)/bin:$(PATH)

TARGET = dsobd_psp
OBJS = build/main.o \
       build/gfx_psp.o \
       build/psp_assets.o \
       build/dtec_audio_psp.o \
       build/dtec_net_psp.o \
       build/dtec_ui_gauges_psp.o \
       build/dtec_ui_screens_psp.o \
       build/dtec_obd.o \
       build/dtec_settings.o \
       build/dtec_logger.o \
       build/ini.o

INCDIR = include ../include ../core/include
CFLAGS = -O2 -G0 -Wall -Wextra -Wno-unused-function -DDTEC_BRAND_NAME=\"PSPOBD\"
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PSP_FW_VERSION = 500

LIBS = -lm -lpspgu -lpspaudio -lpsppower -lpsputility -lpspwlan -lpspnet_inet

EXTRA_TARGETS = EBOOT.PBP
EXTRA_CLEAN = build/*.o
PSP_EBOOT_TITLE = PSPOBD
PSP_EBOOT_ICON = assets/ICON0.PNG
PSP_EBOOT_PIC1 = assets/PIC1.PNG

PSPSDK = $(PSPDEV)/psp/sdk
include $(PSPSDK)/lib/build.mak

CC       = $(PSPDEV)/bin/psp-gcc
CXX      = $(PSPDEV)/bin/psp-g++
AS       = $(PSPDEV)/bin/psp-gcc
LD       = $(PSPDEV)/bin/psp-gcc
AR       = $(PSPDEV)/bin/psp-gcc-ar
RANLIB   = $(PSPDEV)/bin/psp-gcc-ranlib
STRIP    = $(PSPDEV)/bin/psp-strip
MKSFO    = $(PSPDEV)/bin/mksfoex
PACK_PBP = $(PSPDEV)/bin/pack-pbp
FIXUP    = $(PSPDEV)/bin/psp-fixup-imports
PRXGEN   = $(PSPDEV)/bin/psp-prxgen
ENC      = $(PSPDEV)/bin/PrxEncrypter

build/%.o: source/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: ../source/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.o: ../core/source/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	@mkdir -p build

%.prx: %.elf
	$(PRXGEN) $< $@

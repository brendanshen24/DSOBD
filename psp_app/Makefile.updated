PSPDEV ?= /Users/brendan/DSCarplay/tools/pspdev
export PATH := $(PSPDEV)/bin:$(PATH)

TARGET = dsobd_psp
OBJS = source/main.o \
       source/config.o \
       source/net.o \
       source/obd.o \
       source/ui.o \
       ../core/source/elm_core.o \
       ../core/source/ini.o

INCDIR = include ../core/include
CFLAGS = -O2 -G0 -Wall -Wno-unused-function
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

BUILD_PRX = 1
PSP_FW_VERSION = 500

LIBS = -lpsppower -lpsputility -lpspwlan -lpspnet_inet

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = DSOBD PSP - OBDCheck VP01 Terminal

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

%.prx: %.elf
	$(PRXGEN) $< $@

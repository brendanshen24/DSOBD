#---------------------------------------------------------------------------------
# DSOBD - Nintendo 3DS OBDCheck VP01 WiFi Terminal Makefile
#---------------------------------------------------------------------------------
.SUFFIXES:

export DEVKITPRO ?= /Users/brendan/DSCarplay/tools/devkitpro
export DEVKITARM ?= $(DEVKITPRO)/devkitARM
export CTRULIB   ?= $(DEVKITPRO)/libctru

export PATH      := $(DEVKITPRO)/tools/bin:$(DEVKITARM)/bin:$(PATH)

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

export TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET          := dsobd
BUILD           := build
SOURCES         := source ../core/source
DATA            := data
INCLUDES        := include ../core/include $(CTRULIB)/include

APP_TITLE       := DSOBD 3DS Terminal
APP_DESCRIPTION := Veepeak OBDCheck VP01 WiFi Terminal
APP_AUTHOR      := DSOBD
export APP_ICON := $(TOPDIR)/icon.png

ARCH            := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS          := -g -Wall -O2 -mword-relocations \
                   -ffunction-sections \
                   $(ARCH) -D__3DS__

CFLAGS          += $(INCLUDE)

CXXFLAGS        := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS         := -g $(ARCH)
LDFLAGS         = -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS            := -lctru -lm
LIBDIRS         := $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT   := $(CURDIR)/$(TARGET)
export TOPDIR   := $(CURDIR)

export VPATH    := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
                   $(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR  := $(CURDIR)/$(BUILD)

CFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES        := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES          := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES        := $(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

ifeq ($(strip $(CPPFILES)),)
    export LD   := $(CC)
else
    export LD   := $(CXX)
endif

export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES_BIN     := $(addsuffix .o,$(BINFILES))
export OFILES         := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES         := $(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE        := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                         $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                         -I$(CURDIR)/$(BUILD)

export LIBPATHS       := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS      := $(if $(NO_SMDH),,$(OUTPUT).smdh)
export _3DSXFLAGS     += --smdh=$(CURDIR)/$(TARGET).smdh

.PHONY: all clean

all: $(BUILD) $(DEPSDIR)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile
	@if [ -f $(CURDIR)/$(TARGET).elf ] && [ -f $(CURDIR)/cia.rsf ]; then \
		$(DEVKITPRO)/tools/bin/makerom -f cia -o $(CURDIR)/$(TARGET).cia -elf $(CURDIR)/$(TARGET).elf -rsf $(CURDIR)/cia.rsf -icon $(CURDIR)/$(TARGET).smdh -exefslogo -target t 2>/dev/null || true; \
		echo "built ... $(TARGET).cia"; \
	fi

$(BUILD):
	@mkdir -p $@

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

clean:
	@echo "Cleaning 3DS build..."
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).map $(TARGET).cia

else

$(OUTPUT).3dsx : $(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf : $(OFILES)

-include $(DEPSDIR)/*.d

endif

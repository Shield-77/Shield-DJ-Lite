ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set. Install devkitPro 3DS development tools first.)
endif

include $(DEVKITARM)/3ds_rules

TARGET      := ShieldDJLite
BUILD       := build
SOURCES     := source
INCLUDES    := include
DATA        :=
GRAPHICS    :=
ROMFS       := romfs

APP_VERSION     := v1.7.6
APP_CIA_VERSION := 1125
APP_TITLE       := Shield DJ Lite by Shield-77
APP_DESCRIPTION := Lecteur audio MP3 et OGG avec pitch et vitesse synchronises
APP_AUTHOR      := Shield-77
APP_PRODUCT_CODE := SHLD3DS01
APP_UNIQUE_ID   := 0xA2101
MAKEROM         ?= makerom
BANNERTOOL      ?= bannertool

ARCH     := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS   := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections -fdata-sections $(ARCH)
CFLAGS   += -I$(DEVKITPRO)/libctru/include -I$(DEVKITPRO)/portlibs/3ds/include -D__3DS__ -D_POSIX_C_SOURCE=200809L -DMINIMP3_NO_SIMD
CXXFLAGS := $(CFLAGS) -std=gnu++11 -fno-rtti -fno-exceptions
LDFLAGS  := -specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(BUILD)/$(TARGET).map
LIBS     := -L$(DEVKITPRO)/libctru/lib -L$(DEVKITPRO)/portlibs/3ds/lib -lctru -lvorbisidec -logg -lm

.PHONY: all cia clean
all: cia

cia: $(BUILD)/$(TARGET).cia

$(BUILD)/$(TARGET).cia: $(BUILD)/$(TARGET).elf Makefile app.rsf icon.png banner.png banner.wav romfs/boot-top.bgr romfs/boot-bottom.bgr
	@mkdir -p $(BUILD)
	@"$(BANNERTOOL)" makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i icon.png -o $(BUILD)/icon.icn
	@"$(BANNERTOOL)" makebanner -i banner.png -a banner.wav -o $(BUILD)/banner.bnr
	@"$(MAKEROM)" -f cia -o "$@.tmp" -target t -ver $(APP_CIA_VERSION) -desc app:4 -rsf app.rsf -exefslogo -elf $(BUILD)/$(TARGET).elf -icon $(BUILD)/icon.icn -banner $(BUILD)/banner.bnr
	@mv -f "$@.tmp" "$@"

$(BUILD)/$(TARGET).elf: $(SOURCES)/main.c include/minimp3.h include/app_wifi_guard.h include/shield_ui_language.h $(DEVKITPRO)/portlibs/3ds/include/tremor/ivorbisfile.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -Iinclude -o $@ $< $(LIBS)

clean:
	@rm -rf $(BUILD)

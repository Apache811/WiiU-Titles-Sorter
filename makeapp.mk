# Wii U application pipeline. Kept independent from the Windows test build.
.SUFFIXES:
ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

.PHONY: all clean rebuild

# The application entry point and WUHB metadata are mandatory. Fail early with
# an actionable message instead of producing a partial RPX.
WIIU_MAIN := $(ROOT_DIR)/src/platform/wiiu/main.cpp
WUHB_META := $(ROOT_DIR)/meta/iconTex.tga $(ROOT_DIR)/meta/bootTvTex.tga $(ROOT_DIR)/meta/bootDrcTex.tga
FONT_ASSET := $(ROOT_DIR)/fonts/DejaVuSans.ttf
ASSET_DIR := $(ROOT_DIR)/build/assets/wiiu
ASSET_SOURCE_NAMES := folder_blue.png folder_green.png folder_yellow.png folder_orange.png folder_red.png folder_rose.png folder_purple.png folder_grey.png ZL.png ZR.png unknown.png lock.png
ASSET_OUTPUTS := $(addprefix $(ASSET_DIR)/,$(ASSET_SOURCE_NAMES))

MISSING_APP_FILES := $(filter-out $(wildcard $(WUHB_META) $(FONT_ASSET) $(WIIU_MAIN)),$(WUHB_META) $(FONT_ASSET) $(WIIU_MAIN))
ifneq ($(strip $(MISSING_APP_FILES)),)
all:
	@echo "ERROR: required Wii U application files are missing:"
	@$(foreach file,$(MISSING_APP_FILES),echo "  $(file)";)
	@exit 1
else
ifeq ($(strip $(DEVKITPRO)),)
$(error DEVKITPRO is not set. Set DEVKITPRO to your devkitPro installation.)
endif
ifeq ($(strip $(DEVKITPPC)),)
$(error DEVKITPPC is not set. Set DEVKITPPC to your devkitPro installation.)
endif

TOPDIR := $(ROOT_DIR)
WUMS_ROOT := $(DEVKITPRO)/wums
APP_NAME := Titles Sorter
APP_SHORTNAME := TitlesSorter
APP_AUTHOR := Apache81
include $(DEVKITPRO)/wut/share/wut_rules

# WUHBUtils is needed to read metadata/icons from SD-resident WUHB bundles.
ifneq ($(wildcard $(WUMS_ROOT)/include/wuhb_utils/utils.h),$(WUMS_ROOT)/include/wuhb_utils/utils.h)
$(error Missing libwuhbutils headers under $(WUMS_ROOT)/include; install wiiu-env/libwuhbutils and its WUHBUtilsModule dependency)
endif
ifneq ($(wildcard $(WUMS_ROOT)/lib/libwuhbutils.a),$(WUMS_ROOT)/lib/libwuhbutils.a)
$(error Missing $(WUMS_ROOT)/lib/libwuhbutils.a; build/install wiiu-env/libwuhbutils with make install)
endif

# Pin the target compiler explicitly. Host UCRT64/MinGW is used only by
# maketests.mk and must never be able to compile/link the Wii U RPX.
export CC := $(DEVKITPPC)/bin/powerpc-eabi-gcc
export CXX := $(DEVKITPPC)/bin/powerpc-eabi-g++
export LD := $(CXX)

TARGET := TitlesSorter
BUILD := build/wiiu
RELEASE_DIR := $(ROOT_DIR)/$(BUILD)/release
SOURCES := src/core src/ui src/ui/sdl src/platform/wiiu
# Embed only generated runtime assets, never the high-resolution source artwork.
DATA := $(ASSET_DIR) $(ROOT_DIR)/fonts
INCLUDES := include
LIBS := -lSDL2_image -lwebp -ljpeg -lSDL2_ttf -lfreetype -lharfbuzz -lpng -lbz2 -lbrotlidec -lbrotlicommon -lSDL2 -lmocha -lwuhbutils -lz -lwut
LIBDIRS := $(PORTLIBS) $(WUT_ROOT) $(WUT_ROOT)/usr $(WUMS_ROOT)

# Compare absolute paths: notdir(CURDIR) is only "wiiu" when BUILD is "build/wiiu",
# which previously made this condition permanently true and recursively
# re-entered the directory target.
BUILD_ABS := $(abspath $(ROOT_DIR)/$(BUILD))
ifneq ($(abspath $(CURDIR)),$(BUILD_ABS))
export OUTPUT := $(ROOT_DIR)/$(BUILD)/$(TARGET)
export TOPDIR := $(ROOT_DIR)
export VPATH := $(foreach dir,$(SOURCES),$(ROOT_DIR)/$(dir)) $(DATA)
export DEPSDIR := $(ROOT_DIR)/$(BUILD)
CFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES := $(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES := $(ASSET_SOURCE_NAMES) $(notdir $(FONT_ASSET))
export LD := $(if $(strip $(CPPFILES)),$(CXX),$(CC))
# WUT's RPX specs provide the __rpx_start entry point and Wii U linker script.
# Omitting RPXSPECS causes ld to fall back to _start and emits the startup warning.
export LDFLAGS := -g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $(OUTPUT)).map
export OFILES_BIN := $(addsuffix .o,$(BINFILES))
export OFILES_SRC := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN := $(addsuffix .h,$(subst .,_,$(BINFILES)))
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(ROOT_DIR)/$(dir)) $(foreach dir,$(LIBDIRS),-I$(dir)/include) -I$(ROOT_DIR)/$(BUILD) -I$(DEVKITPRO)/portlibs/wiiu/include/SDL2
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export CFLAGS := -g -Wall -Wextra -Wpedantic -Werror -O2 -ffunction-sections $(MACHDEP) $(INCLUDE) -D__WIIU__ -D__WUT__
export CXXFLAGS := $(CFLAGS) -std=gnu++20 -fno-exceptions -fno-rtti
export ASFLAGS := -g $(ARCH)
export APP_ICON := $(TOPDIR)/meta/iconTex.tga
export APP_TV_SPLASH := $(TOPDIR)/meta/bootTvTex.tga
export APP_DRC_SPLASH := $(TOPDIR)/meta/bootDrcTex.tga

.PHONY: all clean rebuild $(BUILD) normalize-timestamps
.NOTPARALLEL: all

# This also protects direct `make -f makeapp.mk` invocations.
all: normalize-timestamps check-wiiu-toolchain check-generated-assets $(BUILD)

normalize-timestamps:
	@bash "$(ROOT_DIR)/tools/normalize_timestamps.sh"

# Resource preparation is deliberately owned by makeresources.mk and the
# top-level dispatcher. This makefile only validates and embeds its outputs.
.PHONY: check-generated-assets
check-generated-assets:
	@for asset in $(ASSET_OUTPUTS); do \
		test -f "$$asset" || { \
			echo "ERROR: generated runtime asset is missing: $$asset"; \
			echo "Run 'make resources' before invoking makeapp.mk directly."; \
			exit 1; \
		}; \
	done

.PHONY: check-wiiu-toolchain
check-wiiu-toolchain:
	@set -eu; \
	compiler="$(DEVKITPPC)/bin/powerpc-eabi-g++"; \
	test -x "$$compiler" || { echo "ERROR: devkitPPC C++ compiler not found: $$compiler"; exit 1; }; \
	target="$$("$$compiler" -dumpmachine)"; \
	test "$$target" = "powerpc-eabi" || { echo "ERROR: expected powerpc-eabi compiler, got: $$target"; exit 1; }; \
	echo "Wii U compiler: $$compiler ($$target)"

$(BUILD):
	@mkdir -p "$@"
	@$(MAKE) --no-print-directory -C "$@" -f "$(ROOT_DIR)/makeapp.mk"
	@test -f "$(ROOT_DIR)/$(BUILD)/$(TARGET).wuhb" || { echo "ERROR: expected WUHB was not produced: $(ROOT_DIR)/$(BUILD)/$(TARGET).wuhb"; exit 1; }
	@mkdir -p "$(RELEASE_DIR)"
	@cp "$(ROOT_DIR)/$(BUILD)/$(TARGET).wuhb" "$(RELEASE_DIR)/$(TARGET).wuhb"
	@echo "Release ready: $(RELEASE_DIR)/$(TARGET).wuhb"
else
DEPENDS := $(OFILES:.o=.d)
.PHONY: all
all: $(OUTPUT).wuhb
$(OUTPUT).wuhb: $(OUTPUT).rpx
$(OUTPUT).rpx: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)
$(OFILES_SRC): $(HFILES_BIN)
%.ttf.o %_ttf.h: %.ttf
	@$(bin2o)
%.png.o %_png.h: %.png
	@$(bin2o)
%.tga.o %_tga.h: %.tga
	@$(bin2o)
-include $(DEPENDS)
endif
endif

clean:
	@rm -rf "$(ROOT_DIR)/build/wiiu"
rebuild:
	@$(MAKE) --no-print-directory -f "$(ROOT_DIR)/makeapp.mk" clean
	@$(MAKE) --no-print-directory -f "$(ROOT_DIR)/makeapp.mk" all

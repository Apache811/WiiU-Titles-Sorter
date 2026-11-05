# Build-time resource import pipeline. This makefile owns generated runtime assets.
.SUFFIXES:
ROOT_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

ASSET_PYTHON ?= auto
ASSET_DIR := $(ROOT_DIR)/build/assets/wiiu
ASSET_STAMP := $(ASSET_DIR)/.prepared
ASSET_SCRIPT := $(ROOT_DIR)/tools/prepare_assets.py
PYTHON_RESOLVER := $(ROOT_DIR)/tools/resolve_python.sh
ASSET_SOURCE_NAMES := folder_blue.png folder_green.png folder_yellow.png folder_orange.png folder_red.png folder_rose.png folder_purple.png folder_grey.png ZL.png ZR.png unknown.png lock.png
ASSET_SOURCES := $(addprefix $(ROOT_DIR)/resources/,$(ASSET_SOURCE_NAMES))
ASSET_OUTPUTS := $(addprefix $(ASSET_DIR)/,$(ASSET_SOURCE_NAMES))

.PHONY: all _all clean rebuild normalize-timestamps
.NOTPARALLEL: all

# Run before Make compares source and generated-file timestamps.
all: normalize-timestamps _all

_all: $(ASSET_STAMP)
	@set -eu; \
	missing=0; \
	for asset in $(ASSET_OUTPUTS); do \
		if test ! -f "$$asset"; then echo "Missing generated asset: $$asset"; missing=1; fi; \
	done; \
	if test "$$missing" -ne 0; then \
		echo "Regenerating the incomplete resource set..."; \
		rm -f "$(ASSET_STAMP)"; \
		$(MAKE) --no-print-directory -f "$(ROOT_DIR)/makeresources.mk" all ASSET_PYTHON="$(ASSET_PYTHON)"; \
	fi

$(ASSET_STAMP): $(ASSET_SOURCES) $(ASSET_SCRIPT) $(PYTHON_RESOLVER)
	@set -eu; \
	python_exe="$$(bash "$(PYTHON_RESOLVER)" "$(ASSET_PYTHON)")" || { \
		echo "ERROR: resource import requires Python 3 and Pillow."; \
		echo "See docs/BUILD_ENVIRONMENT.md (Python asset tooling)."; \
		exit 1; \
	}; \
	echo "Resource importer: $$python_exe"; \
	mkdir -p "$(ASSET_DIR)"; \
	"$$python_exe" "$(ASSET_SCRIPT)" --input "$(ROOT_DIR)/resources" --output "$(ASSET_DIR)"; \
	touch "$@"

normalize-timestamps:
	@bash "$(ROOT_DIR)/tools/normalize_timestamps.sh"

clean:
	@rm -rf "$(ASSET_DIR)"

rebuild:
	@$(MAKE) --no-print-directory -f "$(ROOT_DIR)/makeresources.mk" clean
	@$(MAKE) --no-print-directory -f "$(ROOT_DIR)/makeresources.mk" all ASSET_PYTHON="$(ASSET_PYTHON)"

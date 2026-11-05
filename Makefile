# Titles Sorter build dispatcher.
# `make` imports/optimises resources first, then builds the Wii U application.

.DEFAULT_GOAL := all
.SUFFIXES:
ASSET_PYTHON ?= auto

.PHONY: all app resources resources-rebuild cleanresources tests dryrun clean cleanapp cleantests rebuild tests-rebuild tests-sanitize normalize-timestamps fix-timestamps help

# Serialise preflight before build sub-makes, including when the caller uses -j.
.NOTPARALLEL: all app resources resources-rebuild tests dryrun rebuild tests-rebuild tests-sanitize

all: normalize-timestamps app

# Keep the two Wii U stages explicit and sequential. Resource import must finish
# before WUT's makeapp.mk scans and embeds the generated runtime PNG files.
app: normalize-timestamps resources
	@$(MAKE) --no-print-directory -f makeapp.mk all

resources: normalize-timestamps
	@$(MAKE) --no-print-directory -f makeresources.mk all ASSET_PYTHON="$(ASSET_PYTHON)"

resources-rebuild: normalize-timestamps
	@$(MAKE) --no-print-directory -f makeresources.mk rebuild ASSET_PYTHON="$(ASSET_PYTHON)"

cleanresources:
	@$(MAKE) --no-print-directory -f makeresources.mk clean

tests: normalize-timestamps
	@$(MAKE) --no-print-directory -f maketests.mk tests

dryrun: normalize-timestamps
	@$(MAKE) --no-print-directory -f maketests.mk dryrun

# Remove only the Windows host-test artefacts.
cleantests:
	@$(MAKE) --no-print-directory -f maketests.mk clean

# Remove only Wii U application objects and final binaries.
cleanapp:
	@$(MAKE) --no-print-directory -f makeapp.mk clean

# Remove the complete build tree, including generated resources.
clean:
	@rm -rf build

rebuild: normalize-timestamps
	@$(MAKE) --no-print-directory cleanapp
	@$(MAKE) --no-print-directory resources-rebuild ASSET_PYTHON="$(ASSET_PYTHON)"
	@$(MAKE) --no-print-directory app ASSET_PYTHON="$(ASSET_PYTHON)"

tests-rebuild: normalize-timestamps
	@$(MAKE) --no-print-directory -f maketests.mk rebuild

tests-sanitize: normalize-timestamps
	@$(MAKE) --no-print-directory -f maketests.mk sanitize

# Shared preflight: repair future timestamps before Make evaluates build inputs.
normalize-timestamps:
	@bash tools/normalize_timestamps.sh

# Backwards-compatible explicit command.
fix-timestamps: normalize-timestamps

help:
	@echo "Titles Sorter build commands:"
	@echo "  make                 Import resources, then build the Wii U application"
	@echo "  make resources       Import/optimise runtime artwork only"
	@echo "  make resources-rebuild  Force resource re-import"
	@echo "  make app             Build the Wii U application (resources first)"
	@echo "  make tests           Build and run all Windows host tests (UCRT64 GCC)"
	@echo "  make dryrun          Build the read-only Home Menu save planner utility"
	@echo "  make cleanresources  Remove generated runtime resources only"
	@echo "  make cleanapp        Remove only Wii U application objects and binaries"
	@echo "  make cleantests      Remove only Windows test artefacts"
	@echo "  make clean           Remove the complete build directory"
	@echo "  make rebuild         Re-import resources and clean/rebuild the Wii U application"
	@echo "  make normalize-timestamps  Repair future-dated files before building"
	@echo "  make fix-timestamps       Alias for normalize-timestamps"
	@echo "  make tests-rebuild   Clean, rebuild and run all host tests"
	@echo "  make tests-sanitize  Build and run host tests with sanitizers"
	@echo "  ASSET_PYTHON=auto    Auto-detect Python 3 + Pillow (default)"

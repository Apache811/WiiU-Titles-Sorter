# Windows host test pipeline. Intended for MSYS2 MSYS shell + UCRT64 GCC.
.SUFFIXES:
UCRT64_ROOT ?= /ucrt64
export PATH := $(UCRT64_ROOT)/bin:$(UCRT64_ROOT)/x86_64-w64-mingw32/bin:$(PATH)
HOST_CXX ?= $(UCRT64_ROOT)/bin/g++.exe
HOST_BUILD ?= build/tests
CXXFLAGS ?= -std=c++20 -O0 -g
WARNINGS := -Wall -Wextra -Wpedantic -Wconversion -Werror
INCLUDES := -Iinclude -Itests/wut_mock -Itests/sdl_mock
COMPILE := $(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $(INCLUDES) -MMD -MP

CORE_SOURCES := \
	src/core/TitlesSorterCore.cpp \
	src/core/WiiUMetadataProvider.cpp \
	src/core/TitlesSorterSession.cpp \
	src/core/TitlesSorterSettingsStore.cpp \
	src/core/WiiUHomeMenuSaveLocator.cpp \
	src/core/WiiUHomeMenuSaveReader.cpp \
	src/core/HomeMenuSaveBackupManager.cpp \
	src/core/HomeMenuSavePatcher.cpp \
	src/ui/TitlesSorterUI.cpp
CORE_OBJECTS := $(patsubst %.cpp,$(HOST_BUILD)/obj/%.cpp.o,$(CORE_SOURCES))

.PHONY: tests dryrun clean rebuild sanitize check-toolchain normalize-timestamps
.NOTPARALLEL: tests dryrun

# Normalize before Make evaluates object/executable freshness.
normalize-timestamps:
	@bash tools/normalize_timestamps.sh

dryrun: normalize-timestamps check-toolchain $(HOST_BUILD)/titles_sorter_dryrun.exe

tests: normalize-timestamps check-toolchain \
	$(HOST_BUILD)/core.exe \
	$(HOST_BUILD)/session.exe \
	$(HOST_BUILD)/wiiu-provider.exe \
	$(HOST_BUILD)/wiiu-coordinator.exe \
	$(HOST_BUILD)/ui.exe \
	$(HOST_BUILD)/renderer.exe \
	$(HOST_BUILD)/wiiu-input.exe \
	$(HOST_BUILD)/save-locator.exe \
	$(HOST_BUILD)/save-reader.exe \
	$(HOST_BUILD)/settings.exe \
	$(HOST_BUILD)/backup-manager.exe \
	$(HOST_BUILD)/save-patcher.exe \
	$(HOST_BUILD)/save-apply-transaction.exe
	@set -e; \
	for test in core session wiiu-provider wiiu-coordinator ui renderer wiiu-input save-locator save-reader settings backup-manager save-patcher save-apply-transaction; do \
		echo; echo "[TEST] $$test"; \
		if test "$$test" = core; then "$(HOST_BUILD)/$$test.exe" tests/fixtures/sample_menu_save.dat; \
		else "$(HOST_BUILD)/$$test.exe"; fi; \
	done; \
	echo; echo "All 13 test executables passed."

check-toolchain:
	@command -v "$(HOST_CXX)" >/dev/null 2>&1 || { \
		echo "ERROR: UCRT64 host compiler not found: $(HOST_CXX)"; \
		echo "Install it in MSYS2 with: pacman -S --needed mingw-w64-ucrt-x86_64-gcc"; exit 1; }

$(HOST_BUILD)/obj/%.cpp.o: %.cpp
	@mkdir -p "$(dir $@)"
	$(COMPILE) -c "$<" -o "$@"

$(HOST_BUILD)/core.exe: $(HOST_BUILD)/obj/tests/CoreTests.cpp.o $(CORE_OBJECTS)
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@
$(HOST_BUILD)/session.exe: $(HOST_BUILD)/obj/tests/SessionTests.cpp.o $(CORE_OBJECTS)
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@
$(HOST_BUILD)/wiiu-provider.exe: $(HOST_BUILD)/obj/tests/WiiUProviderTests.cpp.o $(HOST_BUILD)/obj/tests/wut_mock/McpMock.cpp.o $(HOST_BUILD)/obj/tests/wut_mock/AcpMock.cpp.o $(HOST_BUILD)/obj/src/platform/wiiu/WiiUInstalledMetadataProvider.cpp.o $(HOST_BUILD)/obj/src/core/TitlesSorterCore.cpp.o $(HOST_BUILD)/obj/src/core/WiiUMetadataProvider.cpp.o
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@
$(HOST_BUILD)/wiiu-coordinator.exe: $(HOST_BUILD)/obj/tests/WiiUCoordinatorTests.cpp.o $(HOST_BUILD)/obj/tests/wut_mock/McpMock.cpp.o $(HOST_BUILD)/obj/tests/wut_mock/AcpMock.cpp.o $(HOST_BUILD)/obj/src/platform/wiiu/WiiUInstalledMetadataProvider.cpp.o $(HOST_BUILD)/obj/src/platform/wiiu/WiiUTitlesSorterCoordinator.cpp.o $(CORE_OBJECTS)
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@
$(HOST_BUILD)/ui.exe: $(HOST_BUILD)/obj/tests/UITests.cpp.o $(CORE_OBJECTS)
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@
$(HOST_BUILD)/renderer.exe: $(HOST_BUILD)/obj/tests/RendererTests.cpp.o $(HOST_BUILD)/obj/tests/sdl_mock/SdlMock.cpp.o $(HOST_BUILD)/obj/src/ui/sdl/TitlesSorterRenderer.cpp.o $(CORE_OBJECTS)
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@
$(HOST_BUILD)/wiiu-input.exe: $(HOST_BUILD)/obj/tests/WiiUInputAdapterTests.cpp.o $(HOST_BUILD)/obj/tests/wut_mock/VpadMock.cpp.o $(HOST_BUILD)/obj/src/platform/wiiu/WiiUInputAdapter.cpp.o $(CORE_OBJECTS)
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@
$(HOST_BUILD)/save-locator.exe: $(HOST_BUILD)/obj/tests/HomeMenuSaveLocatorTests.cpp.o $(HOST_BUILD)/obj/src/core/WiiUHomeMenuSaveLocator.cpp.o
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@
$(HOST_BUILD)/save-reader.exe: $(HOST_BUILD)/obj/tests/HomeMenuSaveReaderTests.cpp.o $(HOST_BUILD)/obj/src/core/WiiUHomeMenuSaveReader.cpp.o $(HOST_BUILD)/obj/src/core/WiiUHomeMenuSaveLocator.cpp.o
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@

-include $(shell test -d $(HOST_BUILD)/obj && find $(HOST_BUILD)/obj -name '*.d' 2>/dev/null)

sanitize: clean
	@$(MAKE) --no-print-directory tests HOST_CXX="$(HOST_CXX)" CXXFLAGS="-std=c++20 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer"
rebuild: clean
	@$(MAKE) --no-print-directory tests HOST_CXX="$(HOST_CXX)"
clean:
	@rm -rf build/tests

$(HOST_BUILD)/backup-manager.exe: $(HOST_BUILD)/obj/tests/HomeMenuSaveBackupManagerTests.cpp.o $(HOST_BUILD)/obj/src/core/HomeMenuSaveBackupManager.cpp.o $(HOST_BUILD)/obj/src/core/TitlesSorterCore.cpp.o
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@

$(HOST_BUILD)/settings.exe: $(HOST_BUILD)/obj/tests/SettingsStoreTests.cpp.o $(HOST_BUILD)/obj/src/core/TitlesSorterSettingsStore.cpp.o $(HOST_BUILD)/obj/src/core/TitlesSorterCore.cpp.o
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@

# Standalone, read-only save-file planner utility (not part of the Wii U binary).
DRYRUN_CORE_OBJECTS := $(filter-out $(HOST_BUILD)/obj/src/ui/TitlesSorterUI.cpp.o,$(CORE_OBJECTS))
$(HOST_BUILD)/titles_sorter_dryrun.exe: $(HOST_BUILD)/obj/src/tools/dryrun_main.cpp.o $(DRYRUN_CORE_OBJECTS)
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@

$(HOST_BUILD)/save-patcher.exe: $(HOST_BUILD)/obj/tests/HomeMenuSavePatcherTests.cpp.o $(HOST_BUILD)/obj/src/core/HomeMenuSavePatcher.cpp.o $(HOST_BUILD)/obj/src/core/TitlesSorterCore.cpp.o
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@

$(HOST_BUILD)/save-apply-transaction.exe: $(HOST_BUILD)/obj/tests/HomeMenuSaveApplyTransactionTests.cpp.o $(HOST_BUILD)/obj/src/core/HomeMenuSaveApplyTransaction.cpp.o $(HOST_BUILD)/obj/src/core/HomeMenuSavePatcher.cpp.o $(HOST_BUILD)/obj/src/core/HomeMenuSaveBackupManager.cpp.o $(HOST_BUILD)/obj/src/core/TitlesSorterCore.cpp.o
	$(HOST_CXX) $(CXXFLAGS) $(WARNINGS) $^ -o $@

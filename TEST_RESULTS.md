# Test and validation record

This file records test evidence for the current v0.5 read-only source snapshot. It distinguishes host-side verification from target and console validation.

## Current source snapshot — 2026-11-04

- Host test suite: **10/10 test executables passed** using GCC 14.2.0, C++20, and strict warning flags (`-Wall -Wextra -Wpedantic -Wconversion -Werror`). Command used in the Linux verification environment: `make tests HOST_CXX=/usr/bin/g++`.
- Resource pipeline: **passed** with Python 3 and Pillow; all bundled runtime artwork was generated and checked.
- Wii U conditional provider and renderer source branches: **syntax-only checks passed** against local mock declarations. These checks do not replace a devkitPPC target build.
- Wii U target cross-build/link: **not performed** for this snapshot.
- Physical Wii U validation: **not performed** for this snapshot. Startup, HOME-menu resume, MCP/ACP metadata, Homebrew on Menu WUHB artwork, Disc/vWii artwork and final UI appearance still require console testing.
- Sanitizer execution: **not validated** in the container because AddressSanitizer could not reserve its shadow-memory range. This is not a passing sanitizer result.

## Release gate

Before publishing a distributable WUHB, the maintainer must build it with the documented devkitPro/WUT environment, run the host tests on the release commit, and smoke-test the resulting application on a physical Wii U running the intended Aroma environment. Record the toolchain/package versions, commit hash, build result, and hardware test result here.

Before publishing the repository publicly, replace `tests/fixtures/sample_menu_save.dat` with a synthetic fixture and retain equivalent integration coverage, or generate the fixture at test runtime. The current fixture is derived from a user-provided sample and must not be made public.

---

# Test results

## Preview metadata and generic artwork — 2026-11-04

- Added the bundled `unknown.png` fallback to the build-time resource pipeline; it is downscaled to 256x256 and embedded in the Wii U binary.
- Removed the `T`, `D` and `W` letter overlays for title/special records when artwork is unavailable; the generic artwork is used instead.
- The coordinator now requests metadata only for title IDs referenced by the current Home Menu save, including installed system-title IDs returned by MCP.
- Homebrew on Menu entries receive the plugin's virtual `iconTex.tga` path in addition to ACP metadata, enabling WUHB artwork redirection through the plugin.
- Added host regression coverage for a requested native system title (Mii Maker), Homebrew virtual icon-path construction, and metadata/icon propagation to a special vWii preview item.
- Asset pipeline: **11/11 PNG assets generated**; folder art 256x235, generic artwork 256x256, ZL/ZR at source dimensions. Source 13,314,779 bytes -> generated 650,652 bytes (95.1% smaller).
- Host test suite: **10/10 test executables passed** with GCC 14.2.0 and `-std=c++20 -O0 -g -Wall -Wextra -Wpedantic -Wconversion -Werror` (`make tests HOST_CXX=/usr/bin/g++`).
- Wii U target cross-build and physical-console validation: **not run**. Actual MCP/ACP responses, the Homebrew on Menu virtual icon path, and renderer appearance must still be verified on the console.

## Settings startup-loading regression fix — 2026-11-04

- Corrected startup ordering: settings are now loaded after `WiiUTitlesSorterCoordinator::Begin()` creates/resets its session and before the interactive UI is entered.
- Extended the coordinator test to verify that settings applied after `Begin()` survive incremental metadata loading.
- Host test suite: **10/10 test executables passed** using GCC 14.2.0 with `-std=c++20 -O0 -g -Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Command: `make tests HOST_CXX=g++`.
- Wii U target cross-build and physical-console validation: **not run** in this environment.

## Latest revision status — build-time asset optimisation (2026-11-04)

- Host test suite: **10/10 test executables passed** using GCC 14.2.0 and the project's strict warning flags.
- Standalone dry-run utility: built successfully and executed against `tests/fixtures/sample_menu_save.dat`; it reported the plan without modifying the input save.
- Asset pipeline: passed with Python 3 and Pillow 12.3.0; all ten generated PNGs passed dimension and alpha-preservation checks.
- Makefile dependency/VPATH flow: inspected with a stub WUT rules file; generated assets and font resolve to the expected 11 embedded objects.
- Wii U target compilation: **not run**; devkitPro/WUT is unavailable in this environment.
- Console validation: **not run**; generated folder artwork, startup memory behaviour, and final appearance still require a real Wii U test.

Earlier sections below document historical revisions and their respective validation status.

## Host test suite

- Compiler: GCC 14.2.0 (Linux host build used for this verification pass)
- Flags: `-std=c++20 -O0 -g -Wall -Wextra -Wpedantic -Wconversion -Werror`
- Command: `make -f maketests.mk tests HOST_CXX=g++ UCRT64_ROOT=/usr`
- Result: **9/9 test executables passed** after the linker/toolchain and memory-bounding changes.
- Session regression coverage: unchanged options reuse the existing preview; changing a sort option requests regeneration.
- Core parser/planner integration: passed with `tests/fixtures/sample_menu_save.dat`
- Renderer regression: includes the `Screen::LoadingData` path.

## Sanitizers

- An AddressSanitizer/UndefinedBehaviorSanitizer build compiled, but execution could not be validated in this container.
- The container injects `LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libjemalloc.so.2`; with that preload removed, ASan still failed to reserve its shadow-memory range (`ReserveShadowMemoryRange`, `errno: 12`).
- This is an environment limitation, **not a passing sanitizer run**.

## Wii U target

- Target build was not run in this environment because devkitPro/WUT for the Wii U target is not installed here.
- `makeapp.mk` now explicitly links with WUT's `$(RPXSPECS)` (which supplies the RPX startup entry point and linker script) and pins CC/CXX/LD to the devkitPPC `powerpc-eabi` toolchain.
- The user previously reported an immediate Wii U system error 160-2203 before any UI appeared. This source revision has not been rebuilt with devkitPPC or tested on hardware.
- Required validation: run `make cleanapp` then `make`, confirm the `_start` warning is gone and the log identifies `powerpc-eabi-g++`, then test the app on the console.

A passing host suite validates portable logic and mocked adapters; it does not validate target linking, actual MCP/ACP behaviour, storage mounts, SDL hardware rendering or runtime behaviour on Wii U.


## v0.14.5 source checks

- Removed `VPAD_BUTTON_HOME` -> `UiInput::Home` mapping.
- Removed application-level HOME exit handling from the UI controller and startup error phase.
- `WHBProcIsRunning()` is now the only normal runtime exit gate.
- Added Titles Info-style timestamped OSReport/UDP log formatting.
- Hardware test of HOME -> Continue not run in this environment.
- Wii U build and host tests not run in this environment.

## UI refinement — 2026-11-04

- Host test suite: **9/9 executables passed** after the OPTIONS/PREVIEW shoulder-button toggle and renderer layout changes.
- Wii U cross-build: **not run in this environment**. The conditional Wii U PNG decoder for the embedded ZL/ZR textures therefore still requires confirmation with the user's devkitPro build.
- Console rendering, typography fit, and the actual appearance of the shoulder-button textures: **not yet visually verified on hardware**.

## 2026-11-04 preview feedback correction

- Removed the custom controller-driven virtual keyboard completely.
- Added `SearchKeyboard`, copied from the working Titles Info implementation: `SDL_WiiUSetSWKBD*`, `SDL_StartTextInput`, VPAD touch forwarding, SDL event watch and `nn::swkbd::GetStateInputForm()` completion handling.
- While the native keyboard is active, normal Titles Sorter VPAD navigation is suppressed; the keyboard receives VPAD touch state and pumps SDL events.
- Excluded Pages opens the native keyboard with the current expression prefilled; confirmed input is validated through `ParseExcludedPages`, cancelled input leaves the existing option unchanged.
- Corrected preview selection rendering to use `selectedPreviewSlot` and draw a visible outline without covering the item artwork.
- Folder entry now remembers the selected Home Menu slot and restores it on B.
- Folder artwork is aspect-fit and centred (no distortion); folder name is bold and 20% larger.
- TGA icon loader now handles descriptor alpha-bit semantics, horizontal/vertical origin, and alternate `/vol/<device>/...` paths. Missing/unreadable icon paths are logged for device-side diagnosis.
- Footer glyph mapping uses E07D, E07E, E041, E000 and E001 as supplied; preview footer shows only Enter Folder on Home and only Back to Home Menu inside a folder.
- Host test suite: **9/9 executables passed**.
- Wii U devkitPPC build and console testing were not available in this environment. Native keyboard API integration, title icon file access and rendered glyphs must be validated on hardware.


## 2026-11-04 preview feedback correction

Host test command: `make tests HOST_CXX=g++ UCRT64_ROOT=/`

Result: all 9 test executables passed. Session tests now explicitly verify that changing Sort By from Title ID to Product Code and Long Name changes both preview labels and planned order.

Not verified: devkitPPC/WUT target build and Wii U console runtime. SDL_image loading, ACP metadata fallback, and glyph rendering require console validation.


## Optimisation pass — build-time artwork pipeline (2026-11-04)

- Host test suite: **10/10 executables passed** using GCC 14.2.0 with `-std=c++20 -O0 -g -Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Asset script: passed with Python 3 and Pillow 12.3.0. All ten generated PNGs were created; folder images are 256x235 and ZL/ZR retain source dimensions.
- Embedded PNG input size: 11,585,567 bytes; generated output: 573,614 bytes (95.0% reduction for these assets).
- Renderer change: removed runtime folder-image scaling and temporary full-resolution SDL surfaces; generated folder PNGs are loaded directly.
- Wii U cross-build: **not run** because devkitPro/WUT is unavailable in this environment. The Makefile integration, generated-object lookup through WUT's VPATH/bin2o rules, and on-console appearance still require validation on the user's machine.
- GPU texture dimensions remain 256x235 as before; this change does not claim a further reduction in final GPU texture memory.


## 2026-11-04 — Disc/vWii classification, lock overlay and SD homebrew metadata

- Fixed the parser's mirrored-record case: when a Disc (`0x05`) or vWii (`0x09`) record coexists with a conflicting record in the other media bank, the parser retains the explicitly typed special kind and title ID instead of becoming `Unknown`.
- Added a regression test for mirrored Disc and vWii records.
- Replaced the `LOCK` text pill with the transparent `resources/lock.png` overlay, prepared to 48x48 and rendered at 30x30 logical pixels in the artwork's top-right corner.
- Added optional in-memory icon bytes to title metadata and preview items so WUHB TGA artwork can be decoded with `SDL_image` without writing cache files to the SD card.
- Homebrew metadata resolution now reproduces Homebrew on Menu's path hash (`37 * hash + byte`) and scans `SD:/wiiu/apps` plus one directory level. WUHB bundles are read with libwuhbutils; RPX sidecar metadata/artwork is used when present. Filename fallback remains available if bundle metadata cannot be read.
- Added the Wii Menu icon path fallback when a vWii metadata record is unavailable.
- Host test suite: **10/10 executables passed** using GCC 14.2.0 with `-std=c++20 -O0 -g -Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Target-branch provider syntax check: passed with a host compiler and test WUT/WUHBUtils declarations.
- Resource pipeline: all 12 PNGs generated successfully; source assets total 14,289,190 bytes and prepared runtime assets total 654,378 bytes (95.4% reduction). `lock.png` output is RGBA 48x48.
- Not run: actual devkitPPC/WUT cross-build and physical-console tests. The target build now requires `libwuhbutils` installed under `$(DEVKITPRO)/wums` and the matching `WUHBUtilsModule` active in Aroma; verify these on the development machine before building.


## Follow-up: lock placement, artwork fallbacks, and sortable Disc/Wii (2026-11-04)

- Increased the lock overlay from 30×30 to 60×60 logical pixels and anchored it to the upper-right corner of the full tile rectangle.
- Added alternate WUHB artwork paths and GamePad/DRC image fallback when no dedicated icon is present.
- Added devoptab/CafeOS path retries for installed title icons, regional Wii Menu icon candidates, and optical-volume Disc icon candidates.
- Updated the read-only planner so Disc and Wii channels sort with ordinary titles; unknown records remain fixed.
- All 10 host test executables passed after the changes, including a new session regression test that verifies Disc/Wii sort order and absence of the lock marker when movable. Tests used the host GCC 14.2.0 runner with `-std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Werror`. The Wii U provider's `__WIIU__` branch also compiled against the project's WUT/WUHB test declarations. The full devkitPPC/WUT link and physical console checks are not yet performed.


## Follow-up: homebrew image decoding, special channel IDs and metadata entities (2026-11-04)

- Added an in-memory TGA decoder fallback for common uncompressed and RLE true-colour/grayscale TGA formats, including orientation and alpha handling. WUHB image selection validates candidate signatures/headers and tries dedicated icon aliases before TV/GamePad artwork.
- Corrected the Wii icon candidates to try OSv0 (`00050010-10004000`) and OSv1 (`00050010-10004001`) before legacy compatibility candidates. Added a nominal Disc system-title path (`00050010-10055000`) before optical-volume paths. Hardware availability of these assets remains to be confirmed.
- Added special-channel fallback metadata for the known Disc and vWii IDs. Disc is localized using the Wii U system language (Italian: `Disco`); both special channels have no Product Code listed, so the preview shows `N/A` for that field and uses Title ID as a stable sorting fallback. Title ID labels no longer depend on metadata being present.
- XML entity decoding now also applies to ACP strings and Homebrew on Menu INI values, including named and numeric entities.
- Added regression tests for entity decoding, special-channel fallback metadata, Title ID/Product Code labels and sort order, and TGA decoder fallback.
- Host test suite: **10/10 passed** using GCC 14.2.0, C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror`. This environment used `/usr/bin/g++` with the project's SDL/WUT test doubles. The provider and renderer `__WIIU__` branches also passed syntax-only compilation against test/mock declarations. Full devkitPPC/WUT linking and physical-console icon validation were not run.

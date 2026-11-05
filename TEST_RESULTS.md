# Test and validation record

This file records test evidence for the current v1.0.0 pre-release candidate and preserves historical milestone results below. It distinguishes host-side verification from target and console validation.

## Current source snapshot — v1.0.0 pre-release optimization audit (2026-11-05)

- Baseline: `TitlesSorter(30)-chronological-backup-order.zip`; audit changes are the current working source after that baseline.
- Targeted host tests passed with Clang 17, C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`: core (1 sample), UI, renderer, backup manager, save patcher and Apply transaction.
- The patcher test was also built and run independently with Clang 17; it reported `HomeMenuSavePatcherTests passed.`
- Strict syntax-only checks passed for the modified session, UI, patcher and UI test translation units. Renderer compiled and its test reported `Renderer tests passed.`
- A complete 13-executable host suite was **not** completed after the optimization changes. Full-suite build attempts exceeded the execution time limit before all executables could be produced; this is not a test failure, but the missing full-suite result remains a release gate.
- Wii U target cross-build and physical-console validation were not performed.
- ASan/UBSan execution and on-console heap/timing profiling were not performed.
- `tests/fixtures/sample_menu_save.dat` is intentionally excluded by `.gitignore` and remains a local-only test input. The core test requires the maintainer to provide it locally; verify with `git check-ignore -v tests/fixtures/sample_menu_save.dat`.

## Release gate

Before publishing a distributable WUHB, run the complete host suite from a clean checkout, build with the documented devkitPro/WUT environment, and smoke-test the resulting application on a physical Wii U running the intended Aroma environment. Record the toolchain/package versions, commit hash, build result and hardware test result. Validate backup/Restore/Apply and recovery on a disposable or independently backed-up save.

## Apply transaction foundation — 2026-11-05

- Host suite: **13/13 test executables passed** with GCC 14.2.0 and `-std=c++20 -O0 -g -Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Added `HomeMenuSaveApplyTransaction`: checks the active save still byte-matches the preview source, validates the in-memory patch candidate, creates an autobackup when enabled, stages and rereads the candidate, preserves the original under a recovery name, commits by sibling rename, verifies the installed bytes and rolls back on verification failure.
- Added `RecoverInterruptedApply`, which restores the preserved original when a previous transaction left a recovery file. The Wii U save acquisition adapter now invokes it on the authoritative active path before reading the save; host reader tests verify callback ordering and path selection.
- Added host tests for automatic backup, stale-save rejection without mutation, apply with autobackup disabled, and recovery from an interrupted rename sequence.
- `makeapp.mk` discovers `src/core/*.cpp` automatically, so the new core source is included in the target source scan without changing the user's main `Makefile`.
- The Wii U target compiler was not available, so target compilation remains unverified. POSIX/newlib file operations, especially `rename` behaviour on Wii U devoptab/FAT volumes and power-loss durability, still require target validation.
- Physical Wii U validation: **not performed**. The binary layout remains experimental; successful parsing does not prove firmware will accept the modified save.

## Release gate

Before publishing a distributable WUHB, the maintainer must build it with the documented devkitPro/WUT environment, run the host tests on the release commit, and smoke-test the resulting application on a physical Wii U running the intended Aroma environment. Record the toolchain/package versions, commit hash, build result, and hardware test result here.

The sample save fixture is intentionally ignored by Git and is not part of the repository publication. A clean clone will need a locally supplied fixture at `tests/fixtures/sample_menu_save.dat` to run the core integration test.

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


## 2026-11-05 — Wii U backup platform integration foundation

- Command: `make -f maketests.mk tests HOST_CXX=g++ HOST_BUILD=build/tests-linux`
- Result: **13/13 host test executables passed** with GCC 14.2.0, C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Added a Restore crash-recovery regression test; the simulated active candidate is replaced by the validated `.restore.previous` original.
- Platform methods are included in `src/platform/wiiu` and are picked up by `makeapp.mk` source discovery. The Wii U platform source itself was not compiled in this environment because WUT/Mocha target headers and devkitPPC are unavailable.
- No Wii U build or console test was performed. The platform API is not yet wired to UI actions.


## 2026-11-05 — APPLY UI integration

- Command: `make -f maketests.mk tests HOST_CXX=g++ HOST_BUILD=build/tests-linux`
- Result: **13/13 host test executables passed** with GCC 14.2.0, C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Extended UI tests cover APPLY/OPTIONS/PREVIEW shoulder navigation, default autobackup values, max-backup numeric validation, manual-backup confirmation, backup selection/Restore confirmation and Apply confirmation.
- Renderer tests passed after adding APPLY panel, scrollable backup selector, confirmation presentation and three-tab top bar.
- Wii U target build not run: `DEVKITPRO` and `DEVKITPPC` are unset and `powerpc-eabi-g++` is not available in this environment. Physical Wii U testing not performed.
- Important: passing host tests does not verify Wii U devoptab/FAT rename semantics, power-loss durability or the experimental save-format assumptions.

## 2026-11-05 — UCRT64 `mkdir` portability correction

- User-reported failure: UCRT64 declares `mkdir(const char*)`, while the backup manager called the POSIX two-argument form.
- Fix: include `<direct.h>` on `_WIN32` and call `_mkdir(path.c_str())`; retain `mkdir(path.c_str(), 0777)` on POSIX/Wii U.
- Command run in the available Linux environment: `make tests HOST_CXX=g++ HOST_BUILD=build/tests-linux`.
- Result: **13/13 test executables passed**, with C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Limitation: this execution environment does not contain MSYS2 UCRT64 GCC, so the exact Windows-native `make tests` rerun was not possible here. The reported signature mismatch is directly corrected, but confirmation on the user's UCRT64 installation is still required.


## 2026-11-05 — Backup-manager test harness portability

- Root cause from the user's UCRT64 run: `HomeMenuSaveBackupManagerTests.cpp` used `std::system("mkdir -p ...")`. Under Windows, `std::system` dispatches to the Windows command interpreter, which rejects the POSIX `-p` argument (`Sintassi del comando errata.`); fixture creation then failed at `assert(out.good())`.
- Replaced shell directory commands with `std::filesystem::create_directories` and `std::filesystem::remove_all`.
- Verification command: `make -f maketests.mk tests HOST_CXX=g++ HOST_BUILD=build/tests-linux`.
- Result: **13/13 host test executables passed** with C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Limitation: this environment does not contain MSYS2 UCRT64, so the exact Windows-native rerun has not yet been performed.

## 2026-11-05 — UCRT64 backup-manager cleanup portability

- User-reported failure: `std::filesystem::remove_all(root, filesystemError)` returned an error at the final test cleanup assertion.
- Root cause: the test kept `recovered` (`std::ifstream`) open after reading `active`, then attempted to delete the test directory. Windows does not permit deleting an open file in this situation.
- Fix: explicitly call `recovered.close()` before the final `remove_all`.
- Verification: all 13 host test executables passed with GCC C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- The MSYS2 UCRT64 run and Wii U target build remain unverified in this environment.


## Wii U target build follow-up
- Diagnosed the reported target compilation failure as a stale/mistyped include path in `WiiUHomeMenuSavePlatform.hpp`. The include now references the existing `WiiUHomeMenuSaveReader.hpp`.
- Host test status remains 13/13 passed in the user's UCRT64 environment. Wii U target compilation must be rerun with devkitPPC; it has not yet been verified after this correction.


## 2026-11-05 — OPTIONS/PREVIEW regression correction

- Restored the v0.5 OPTIONS interaction and bottom-bar presentation; the APPLY panel retains its current controls.
- Restored immediate Preview generation on entry (ZR from OPTIONS, ZL from APPLY); no A press is required.
- UI tests now assert the explicit routes: OPTIONS (ZL → APPLY, ZR → PREVIEW), PREVIEW (ZL → OPTIONS, ZR → APPLY), APPLY (ZL → PREVIEW, ZR → OPTIONS).
- Command: `make -f maketests.mk tests HOST_CXX=g++ HOST_BUILD=build/tests-ui-restore`
- Result: **13/13 host test executables passed** with GCC C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- No Wii U target build or physical-console test was performed for this UI correction.


## 2026-11-05 — APPLY layout and confirmation refinement

- Command: `make -f maketests.mk tests HOST_CXX=g++ HOST_BUILD=build/tests-linux`
- Result: **13/13 host test executables passed** with GCC C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- UI coverage verifies Autobackup is changed only by Left/Right, Create Backup opens a dedicated modal, modal navigation/shoulder input is ignored, B cancels and A confirms, and Apply uses the same A/B modal behaviour.
- Renderer coverage exercises the Create Backup and Apply confirmation frames.
- Wii U target build and physical-console testing were not performed.

## 2026-11-05 — APPLY operation-result and Restore decision flow

- Command: `make -f maketests.mk tests HOST_CXX=g++ HOST_BUILD=build/tests-linux`
- Result: **13/13 host test executables passed** with GCC C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Added controller coverage for immediate Create Backup dispatch, success/failure result frames, A-only acknowledgement, Restore decision with selected full path, B returning to the selector, A dispatching Restore, success/failure Restore results, delayed reload until acknowledgement, and Apply decision A/B behaviour.
- Added renderer coverage for decision and operation-result frames and their custom bottom-bar mode.
- Wii U target build and physical-console testing were not performed; devkitPPC is unavailable in this environment.

## 2026-11-05 — APPLY modal typography and footer refinement

- Command: `make -f maketests.mk tests HOST_CXX=g++ HOST_BUILD=build/tests-linux`
- Result: **13/13 host test executables passed** with GCC C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Renderer changes: no confirmation/result border panel; decision fonts 2x (42→84, detail 30→60); result fonts 2x (48→96, detail 28→56); decision footer now includes B/Cancel in addition to centred HOME/Exit and A/Confirm. Result footer remains HOME/Exit plus A/Confirm, with B disabled.
- Main APPLY panel: first four controls shifted down 25 logical pixels; Apply control retains its previous Y position.
- Wii U target build and physical-console testing were not performed.


## 2026-11-05 — APPLY confirmation typography +25%

- Renderer font sizes updated: Create Backup result 48→60 px and detail 28→35 px; Restore decision 42→53 px and selected path 30→38 px; Apply decision 42→53 px.
- Verification: `TitlesSorterRenderer.cpp` compiled successfully using GCC C++20 with `-Wall -Wextra -Wpedantic -Wconversion -Werror`. The full 13-executable suite was not completed in this environment.
- Wii U cross-build and physical-console validation were not performed.

## Apply result lifecycle correction
- UI tests cover Apply success/failure result states, autobackup-path reporting, A-only acknowledgement, and deferred reload request on success only.
- Renderer tests cover the new Apply result screen.
- Target Wii U build and hardware validation remain pending.

## Renderer present-count test correction — 2026-11-05

- User-reported failure: `RendererTests.cpp:114`, expected `GetSdlMockStats().presentCalls == 10U` after drawing the Preview frame.
- Root cause: the test had already drawn ten frames (including the new `PreviewRequired` modal), then drew the Preview frame, which correctly increments `SDL_RenderPresent` to eleven. The final assertion was not updated for that additional draw.
- Fix: changed only the final Preview-frame assertion to `11U`. The earlier assertion after `Screen::GeneratingPreview` remains `10U`.
- Verification: rebuilt and ran the renderer test with GCC C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`; result: **passed** (`Renderer tests passed.`).
- This was a test expectation correction; no renderer production code was changed. The complete 13-executable suite, Wii U target build, and physical-console test were not run as part of this correction.

## 2026-11-05 — Mixed backup chronological ordering

- `HomeMenuSaveBackupManagerTests` passed after adding a regression test with manual and automatic backups interleaved by timestamp.
- Verified the enumeration order is newest-first across both backup types, rather than lexicographic order of the full filename (which grouped the two prefixes incorrectly).
- Compiler: GCC C++20 with `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- The complete 13-executable host suite, Wii U target build and physical-console test were not run for this increment.


## 2026-11-05 — v1.0.0 pre-release optimization audit

- Targeted tests: core, UI, renderer, backup manager, patcher and Apply transaction passed with Clang 17 / C++20 / strict warnings.
- Full suite: not completed after the optimization pass; compilation/linking exceeded the available command time limit.
- Wii U cross-build, physical-console testing, sanitizer execution and memory/frame-time profiling: not performed.


## Restore autobackup change

- `HomeMenuSaveBackupManagerTests`: passed with GCC 14.2.0, C++20, `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- Coverage verifies that the pre-Restore active save is preserved byte-for-byte in an automatic backup before the selected backup is installed.
- The selected backup is loaded before automatic-backup retention runs, so retention cannot remove the file being restored during the operation.
- UI test coverage was added for displaying the autobackup path on a successful Restore result, but the full UI test executable did not finish compiling within this environment's time limit.
- Wii U platform build and hardware testing have not been performed.


## 2026-11-05 — Restore autobackup targeted follow-up

- Built `HomeMenuSaveBackupManagerTests` with GCC 14.2.0, C++20, `-O2 -Wall -Wextra -Wpedantic -Wconversion -Werror`; executable passed.
- Added coverage proving a selected automatic backup can be removed by the retention pass while the staged Restore still completes with the correct bytes.
- Added coverage proving an invalid automatic-retention limit causes pre-Restore autobackup creation to fail, Restore to abort, the staging file to be removed, and the active save to remain byte-for-byte unchanged.
- The targeted test uses the existing supplied sample fixture. No complete 13-executable suite, Wii U cross-build, sanitizer run, or physical-console test was performed for this increment.

- Follow-up validation: the complete 13-executable host suite subsequently passed using native GCC 14.2.0 / Linux, C++20 `-O0`, and `-Wall -Wextra -Wpedantic -Wconversion -Werror`. Command: `make -f maketests.mk tests HOST_CXX=g++ HOST_BUILD=/tmp/ts32-full CXXFLAGS='-std=c++20 -O0'`. This validates the complete host suite in this container, but is not a substitute for rerunning the project's documented MSYS2 UCRT64 command on the release workstation.


## Windows UCRT64 backup-manager test cleanup fix — 2026-11-05

- Fixed the backup-manager test cleanup failure reported by the maintainer at `std::filesystem::remove_all` (previously asserted at test line 214).
- The test now explicitly closes the streams that read the preserved autobackup and recovered active save before removing the temporary test directory. This is required on Windows, where open file handles can prevent directory/file deletion; POSIX may allow unlinking open files and therefore did not expose the issue.
- Cleanup now uses a dedicated `std::error_code` and prints its message if removal fails, so future failures identify the actual filesystem error rather than inheriting or obscuring an earlier error state.
- The fixture remains excluded by `.gitignore`; no `LICENSE` file is added, as requested by the maintainer.
- Targeted `backup-manager.exe` was rebuilt and passed with Clang 17/C++20 and strict warnings in the available Linux environment. A full-suite build attempt timed out while compiling `HomeMenuSavePatcher.cpp`; the Windows-specific UCRT64 execution is still pending.

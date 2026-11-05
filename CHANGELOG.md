# Changelog

Notable project changes are recorded here. Version numbers follow the `MAJOR.MINOR.PATCH` format. **v1.0.0 is the completed, validated and published release.** Entries below v1.0.0 document the chronological development history; any limitations recorded there describe the snapshot at that time, not the published release.

## [1.0.0] — 2026-11-05 — Complete release

### Included
- Complete OPTIONS, PREVIEW and APPLY workflow.
- Home Menu save discovery, parsing, sorting configuration and preview, including folder navigation.
- Installed-title and Homebrew on Menu metadata and artwork handling, including Disc/vWii special cases and generic/lock artwork.
- Separate settings persistence.
- Byte-preserving manual and automatic backups, automatic-backup retention, and a combined newest-first Restore selector.
- Optional pre-Restore autobackup when Autobackup is enabled; Restore is cancelled if that safety backup cannot be created.
- Staged Restore and Apply operations with verification and recovery handling.
- Result screens and post-operation session reload after acknowledgement.
- Host test suite, build/resource pipeline and project documentation.

### Release
- v1.0.0 was validated and published by the maintainer.
- `tests/fixtures/sample_menu_save.dat` remains local-only and excluded by `.gitignore`.
- No `LICENSE` file is included, by maintainer decision.

## [0.5.0] — 2026-11-05 — Read-only preview milestone

### Added
- Read and inspect the current Wii U HOME Menu layout.
- Configure title sorting and excluded pages, then preview the proposed arrangement without applying it.
- Navigate the main preview and inspect folder contents.
- Persist Titles Sorter application settings separately from the HOME Menu save.
- Retrieve installed-title and Homebrew on Menu metadata and artwork where the platform exposes the required information.
- Provide Disc and vWii special-title metadata fallbacks, including localized Disc naming and deterministic sorting fallbacks.
- Decode XML entities in native metadata, ACP fallbacks and Homebrew metadata values.
- Include generic fallback artwork and a lock overlay for fixed entries.
- Provide host-side tests and documented build/resource pipelines.

### Safety and scope
- The application does not write, replace, or apply changes to the HOME Menu save.
- v1.0 is reserved for a future write/apply implementation after the save format and safe replacement process have been validated.

### Known limitations
- The Wii U target cross-build and physical-console validation were not completed for the source snapshot represented by this changelog. A release maintainer must complete both before distributing a WUHB binary.
- Some metadata and artwork paths, including WUHB icon extraction and Disc/vWii artwork, require confirmation on physical hardware.
- The HOME Menu save format and page/slot mapping are only partially understood.
- The development snapshot contains a user-derived save fixture that must be replaced with synthetic test data before the repository is made public.

[0.5.0]: https://github.com/

## Unreleased — v1.0 foundation

- Added settings format v2 with `autobackup=1` and `max_autobackups=10` defaults.
- Kept backward compatibility with settings v1; v1 files receive the new defaults.
- Added a byte-for-byte backup manager with timestamped manual/automatic names,
  validated backup copies, automatic-backup-only retention, backup enumeration,
  and staged restore with rollback.
- Restore deliberately does not create an autobackup, per the agreed workflow.
- Added host coverage for manual backup, backup listing, and restore.
- The Wii U APPLY/RESTORE UI and Wii U filesystem integration are not included
  in this increment; Home Menu save patching remains disabled pending format
  invariants and binary-diff validation.


## 2026-11-05 — In-memory Home Menu save patcher foundation

- Added `HomeMenuSavePatcher`, a filesystem-independent service that applies a validated preview assignment map to a copy of the original save bytes.
- Copies full NAND/USB record pairs and preserves the original bytes of empty records when they move into vacated slots.
- Rejects invalid mappings, duplicate sources, discarded occupied slots, pinned/unknown-slot moves, tampered source models and invalid folder targets.
- Reparses the candidate and verifies that bytes outside recognised slot-record banks remain unchanged, including folder metadata, selection and opaque regions.
- Added synthetic tests for main-menu and folder sorting, empty-record preservation, pinned unknown records, metadata/opaque-byte preservation and invalid input rejection.
- Host suite: **12/12 test executables passed** with GCC 14.2.0, C++20 and strict warnings.
- No Wii U target build or physical-console test was performed. The patcher is not connected to the active-save filesystem or APPLY UI and does not write the active save.


## 2026-11-05 — Apply transaction foundation

- Added `HomeMenuSaveApplyTransaction` to validate the active save against the exact preview source and build/validate the patched candidate before any filesystem mutation.
- When enabled, creates and verifies an autobackup before staging the candidate; when disabled, does not create the backup directory.
- Stages the candidate beside the active save, rereads and validates it, renames the original to a recovery path, commits the candidate, verifies the installed bytes, and attempts rollback if verification fails.
- Added `RecoverInterruptedApply` to restore the original save if startup finds an unfinished transaction. The Wii U save acquisition pipeline invokes it on the resolved authoritative path before reading the save.
- Added host tests for autobackup, stale-preview protection, disabled autobackup and interrupted-transaction recovery.
- Host suite: **13/13 test executables passed** using GCC 14.2.0 and strict C++20 warnings.
- No devkitPPC build or physical Wii U test was performed. `fflush`/close and rename behaviour on the Wii U storage stack, including power-loss durability, remains unverified.


## 2026-11-05 — Wii U backup platform integration foundation

- Added explicit `WiiUHomeMenuSavePlatform` operations for manual backup, backup enumeration, Restore and Apply, using the resolved authoritative active save path.
- Standardized backup storage under `fs:/vol/external01/wiiu/apps/TitlesSorter/backups`.
- Added `RecoverInterruptedRestore` and invoked it before Apply recovery and before startup reads/parses the active save.
- Added a regression test simulating a power interruption after the current save was moved aside during Restore; startup recovery restores the original byte-for-byte.
- Host test suite: 13/13 executables passed with GCC C++20 and strict warnings.
- Not yet implemented: UI wiring/confirmation, backup selector, keyboard field, session reload after Restore/Apply, devkitPPC build and Wii U hardware verification.


## 2026-11-05 — APPLY / OPTIONS / PREVIEW UI integration

### Added
- Added the APPLY top-bar section and ZL/ZR cycling across APPLY, OPTIONS and PREVIEW. The expanded top-bar group grows to the left while preserving the previous ZR button X coordinate.
- Added APPLY rows for Autobackup, maximum automatic backups, manual backup, Restore Backup and Apply the Previewed Layout.
- Added a Wii U software-keyboard input for the autobackup retention count (1–100); invalid input leaves the previous valid value unchanged.
- Added a newest-first backup selector with D-Pad navigation and explicit two-step Restore confirmation.
- Added two-step confirmation for manual backup creation and Apply.
- Connected UI actions to `WiiUHomeMenuSavePlatform`; manual backup refreshes the selector, and successful Apply/Restore returns to save acquisition and reloads the session.
- Made `TitlesSorterSession::Plan()` return a plan only while the sorting options and metadata reference still match the values used to generate it, preventing Apply of a stale preview.
- Added the “Select” action to the new APPLY panel while retaining the original OPTIONS bottom-bar action.

### Validation
- Host suite: **13/13 test executables passed** with GCC 14.2.0, C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`.
- UI regression coverage includes three-section navigation, autobackup defaults, retention input validation, manual-backup confirmation, backup selection and Restore confirmation, and Apply confirmation.
- The Wii U target build was not run: this environment has no `DEVKITPRO`, `DEVKITPPC` or `powerpc-eabi-g++`. No physical-console test was performed. The current source is an experimental v1.0 development increment, not a release candidate.

## 2026-11-05 — Windows UCRT64 host-test portability fix

- Fixed `HomeMenuSaveBackupManager.cpp` directory creation for Windows host tests: use MinGW/UCRT's one-argument `_mkdir` under `_WIN32`, retaining the two-argument POSIX `mkdir(path, mode)` for Wii U and other POSIX targets.
- Split directory inspection errors from directory creation errors so a failed `stat` other than `ENOENT` is not misreported as a creation failure.
- Re-ran all 13 host test executables with GCC 14.2.0 on the available Linux host and strict warnings; all passed.
- The user's UCRT64 `make tests` failure was reproduced by inspection and the exact compile error is addressed. UCRT64 itself is not installed in this execution environment, so the final Windows-native rerun remains to be confirmed by the user.


## 2026-11-05 — UCRT64 backup-manager test harness portability fix

- Removed shell-based `system("mkdir -p ...")` and `system("rmdir ...")` calls from `HomeMenuSaveBackupManagerTests`. On Windows, `std::system` invokes the Windows command interpreter, where the POSIX `mkdir -p` syntax is invalid; this caused the test fixture setup to fail after all compilation steps had succeeded.
- Replaced setup and cleanup with `std::filesystem::create_directories` and `std::filesystem::remove_all`, with explicit error-code assertions. The test no longer depends on shell-specific directory commands.
- Rebuilt and ran all 13 host test executables on the available Linux host with C++20 and strict warnings; **13/13 passed**.
- MSYS2 UCRT64 is not available in this execution environment, so the user's exact `make tests` rerun is still required to confirm the Windows host path.

## 2026-11-05 — UCRT64 backup-manager cleanup portability

- Follow-up UCRT64 failure occurred during `std::filesystem::remove_all` at test cleanup. The recovered-save `std::ifstream` remained open while the test attempted to delete its containing directory.
- Explicitly close the recovered-save stream immediately after reading it and before removing the test directory. This mirrors the earlier Windows rename fix: test files must not remain open across filesystem mutation on Windows.
- Re-ran all 13 host test executables with GCC C++20 and strict warnings; all passed. The exact UCRT64 rerun remains pending user confirmation.


## Wii U build include correction
- Fixed the platform adapter header to include `core/WiiUHomeMenuSaveReader.hpp`, matching the actual reader header filename. This resolves the target compiler fatal include error without changing build flags or architecture.


## 2026-11-05 — UI regression correction: restore v0.5 OPTIONS/PREVIEW behaviour

- Restored OPTIONS input handling and presentation to the v0.5 behaviour; its sorting controls, Excluded Pages interaction and original bottom-bar labels are unchanged.
- Restored immediate Preview generation when entering PREVIEW. Pressing A is no longer required to create a preview. Preview grid, page navigation, folder entry and folder return remain unchanged.
- Set the top-bar order to OPTIONS → PREVIEW → APPLY.
- Implemented the requested explicit shoulder routes: OPTIONS (ZL → APPLY, ZR → PREVIEW), PREVIEW (ZL → OPTIONS, ZR → APPLY), APPLY (ZL → PREVIEW, ZR → OPTIONS).
- Kept the current APPLY panel and its backup/restore/apply actions intact.
- Added UI regression tests for all six shoulder transitions and immediate Preview generation.
- Host test suite: **13/13 executables passed** with GCC C++20 and strict warnings. No Wii U target build or console test was performed as part of this UI-only correction.


## 2026-11-05 — APPLY central panel and confirmation UI refinement

- Reworked only the APPLY central panel to follow the supplied mock-up and OPTIONS control styling: matching heading/subtitle sizes, Sort By-style Autobackup selector with `< Enabled >` / `< Disabled >`, and an Excluded Pages-style numeric field showing its current value directly.
- Added the requested 70 px vertical gap after the maximum-autobackups field. Create Backup and Restore Backup now share the available control width in two equal side-by-side controls; Apply the Previewed Layout is centred below at the same width.
- Removed the inline “press A again” prompts for Create Backup and Apply. Each action now opens a dedicated centred confirmation frame. Create uses A / OK; Apply uses A / Proceed and B / Cancel. These frames hide the bottom bar and ignore every input except A and B.
- Autobackup now changes only with D-Pad Left/Right and displays its value between chevrons; A no longer toggles it.
- Split the Restore Backup introduction into two lines. Kept the Restore selector and its B / Cancel bottom-bar action.
- APPLY bottom bar follows OPTIONS' layout and glyph placement, changing only the right-hand action label to Select.
- Added UI tests for modal input isolation, A/B confirmation/cancellation, and Autobackup left/right-only behaviour; added renderer coverage for both confirmation frames.
- Verification: all **13/13 host test executables passed** with GCC C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`. No devkitPPC build or physical-console test was performed.

## 2026-11-05 — APPLY backup/restore operation flow and vertical control layout

- Reworked the central APPLY panel into five full-width controls with identical outer dimensions: Autobackup, Max Number of Autobackups to Keep, Create Backup, Restore Backup, and Apply the Previewed Layout. Autobackup and the numeric field are vertically aligned; the Max-to-Create and Restore-to-Apply gaps are 70 logical pixels.
- Create Backup now starts immediately on the first A press. Its result frame reports `Backup Completed !!!` with the created path, or `Backup Failed !!!` with the operation error. The result frame has a bottom bar with centred HOME/Exit and right-aligned A/Confirm; only A and HOME are accepted.
- Selecting a backup now opens a dedicated Restore decision frame with the selected full path. A starts Restore; B returns to the backup selector. The operation then displays `Backup Restored !!!` or `Restore Failed !!!` with the error. The result frame accepts only A and HOME; a successful restore reloads Home Menu data after the user acknowledges the result.
- Apply now uses a dedicated decision frame with the requested wording. A proceeds and B returns to APPLY. The main APPLY bottom bar and top bar are unchanged.
- Added renderer and controller tests for success/failure result states, immediate Create Backup request, Restore decision/cancel/execute/result/acknowledgement, Apply decision/cancel/execute, and modal input restrictions.
- Verification: all **13/13 host test executables passed** with GCC C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`. The Wii U target build and physical-console behaviour have not been verified in this environment.

## 2026-11-05 — APPLY modal typography, footer and vertical alignment refinement

- Shifted the Autobackup, maximum-autobackups, Create Backup and Restore Backup controls down by 25 logical pixels; the Apply control remains at its previous vertical position.
- Removed the rounded confirmation/result panel so modal messages render directly on the screen background.
- Doubled decision message font size from 42 to 84 and selected-backup path font size from 30 to 60. Doubled operation-result message font size from 48 to 96 and detail/path font size from 28 to 56.
- Decision frames now show the HOME/Exit action centred, A/Confirm in the right-hand region and B/Cancel at the same right-side anchor used by the backup selector. Operation-result frames retain HOME/Exit and A/Confirm only; B remains disabled there.
- Host verification: all 13 test executables passed with GCC C++20 and strict warnings. No Wii U target build or hardware test was performed.


## 2026-11-05 — APPLY confirmation typography +25%

- Increased Create Backup result message font from 48 to 60 px (+25%) and its path/error detail from 28 to 35 px (+25%).
- Increased Restore decision message font from 42 to 53 px and selected-backup path from 30 to 38 px (rounded to the nearest whole pixel, approximately +25%).
- Increased Apply decision message font from 42 to 53 px (rounded to the nearest whole pixel, approximately +25%).
- Preserved the two-line Apply decision wording, modal backgrounds, footer layouts, control geometry, OPTIONS and PREVIEW.
- Verification: `TitlesSorterRenderer.cpp` compiled successfully with GCC C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`. The full 13-executable suite was not completed in this environment. No Wii U target build or physical-console test was performed.

## 1.0 UI / Apply result lifecycle correction
- Apply now reports an explicit `Layout Applied !!!` or `Apply Failed !!!` result screen using the same operation-result presentation as manual backup.
- When an automatic backup was created, its path is included in the Apply result details, including when a later Apply stage fails.
- A successful Apply no longer triggers a Home Menu reload immediately. The active save and installed-title metadata are re-read only after the user acknowledges the result screen with A.
- A failed Apply remains on its result screen until acknowledged; A then triggers the same reload as a successful Apply.
- Clarified the loading screen copy: it reads the active Home Menu save and installed-title metadata; it is not a save-progress screen.

## 2026-11-05 — Renderer test present-count correction

- Corrected the renderer test's final `SDL_RenderPresent` expectation from 10 to 11. The test draws ten frames before entering the Preview screen, then draws the Preview frame itself, which correctly produces the eleventh present call.
- No renderer implementation change was needed; the failure was an outdated test assertion introduced when the `PreviewRequired` modal coverage added one additional draw.
- Verified the renderer test executable with GCC C++20 and `-Wall -Wextra -Wpedantic -Wconversion -Werror`: **passed** (`Renderer tests passed.`).
- No Wii U target build or physical-console test was performed.

## 2026-11-05 — Unified chronological Restore backup ordering

- Corrected backup enumeration to compare the timestamp portion of each filename, independent of whether it is a manual (`backup_`) or automatic (`autobackup_`) backup.
- Restore selector input now receives one newest-first chronological list across both backup types; backup kind no longer groups entries ahead of timestamp order.
- Added a mixed manual/automatic regression test covering interleaved dates.
- Backup manager host test passed with GCC C++20 and strict warnings (`-Wall -Wextra -Wpedantic -Wconversion -Werror`). Full suite, Wii U target build and physical-console validation were not run for this change.


## 2026-11-05 — Pre-release optimization and consistency audit (v1.0.0 candidate)

- Removed the unused `TitlesSorterSession::BuildPreviewLabels()` path and its 300-string cache. Preview labels are produced directly for the visible page by `GetPreviewPage`; the old cache was never read and caused unnecessary string allocations during save load, metadata updates and preview generation.
- Removed obsolete UI state from the previous confirmation flow (`confirmationPending`, `restoreSucceeded` and `applySucceeded`) and unused `UiFrame` request fields. The preview-required result now uses its dedicated helper, keeping message/footer state consistent.
- Replaced per-call dynamic assignment-validation containers in `HomeMenuSavePatcher` with bounded fixed-size arrays. The assignment count is checked against the maximum supported slot count.
- Removed temporary plan-array copies and dynamic validation bitsets in `BuildDryRunPlan`; fixed-size plan arrays are validated in place.
- Reduced Apply/backup peak memory by releasing full-save readback buffers as soon as their verification role is complete, and by releasing the source-model validation parse before parsing the patched candidate.
- Removed an unnecessary lambda capture in the renderer found by Clang's stricter warning diagnostics. No visual geometry, wording, colours, input routing or lifecycle behaviour was intentionally changed.
- Updated current workflow documentation: Create Backup is immediate; Apply/Restore result screens reload only after A acknowledgement, after both success and failure; backup selection is newest-first across both backup kinds.
- Targeted host verification: core, UI, renderer, backup-manager, patcher and Apply-transaction tests passed with Clang C++20 and strict warnings. A complete 13-executable rerun, Wii U target build and physical-console test were not completed in this environment.

- Restore follows the same rule: after either success or failure, A acknowledges the result and then triggers a reload.


## Restore autobackup safety

- When the Autobackup setting is enabled, Restore now creates an automatic backup of the currently active Home Menu save before replacing it.
- Restore is aborted if the safety autobackup cannot be created.
- The selected backup is loaded before autobackup retention runs, preventing retention from deleting the backup currently being restored.
- The Restore result screen reports the created autobackup path on success and on a subsequent Restore failure.


## 2026-11-05 — Restore autobackup and release-readiness follow-up

- Restore now stages and verifies the selected backup before running automatic retention, then creates a byte-for-byte autobackup of the current active save when the Autobackup option is enabled.
- If the pre-Restore autobackup cannot be completed, Restore aborts, removes its staging file and leaves the active save unchanged. With Autobackup disabled, Restore does not create an automatic backup.
- Reduced overlapping full-save buffers during Restore: the selected backup's in-memory copies are released before autobackup creation; the verified staging file is re-read afterward for final byte-for-byte installation verification.
- Added regression coverage for a selected automatic backup pruned by retention and for autobackup failure aborting Restore safely.
- Updated README, lifecycle, platform, feature-status, UI-contract, memory-audit and release-checklist documentation to describe the current Restore workflow.


## 2026-11-05 — Windows UCRT64 backup-manager test cleanup fix

- Fixed the backup-manager test cleanup failure at `std::filesystem::remove_all`: explicitly close the streams that read the preserved autobackup and recovered active save before removing the temporary test directory.
- Use a dedicated cleanup `std::error_code` and print its message on failure, so the test reports the actual cleanup error.
- No production backup/Restore behaviour changed in this correction.
- The local `sample_menu_save.dat` fixture remains excluded by `.gitignore`; no `LICENSE` file is added, per maintainer decision.
- Targeted backup-manager test passed with Clang 17/C++20 and strict warnings in the available Linux environment. Full suite build timed out in this environment; rerun `make tests` in MSYS2 UCRT64 to verify the Windows-specific cleanup path.

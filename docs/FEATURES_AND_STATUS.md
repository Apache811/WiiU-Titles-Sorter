# Features and implementation status

This document records the intended scope and current implementation boundaries. It should be updated when a feature is added, removed, or validated on hardware.

## Release status — v1.0.0 pre-release candidate

The v0.5.0 read-only milestone remains the previous public feature baseline. The current source contains the complete v1.0 workflow: backup, Restore and Apply actions are wired to the Wii U UI and platform adapter. This is a pre-release candidate only; devkitPPC target compilation, Wii U filesystem validation, save-format invariants and physical-console testing remain release gates.

The preview remains a proposed layout until Apply succeeds. Apply and Restore now mutate the active save through staged replacement; treat them as experimental until the Wii U target and real-save invariants have been verified.

## Milestone scope

- **v0.5:** feature-complete read-only workflow: load and inspect the Home Menu, configure sorting, generate the preview, resolve available title metadata/artwork, and persist Titles Sorter settings separately. No Home Menu save bytes are written.
- **v1.0.0 candidate:** includes settings v2, byte-preserving manual/automatic backups, automatic retention, newest-first mixed backup selection, optional pre-Restore autobackup controlled by the Autobackup setting, an in-memory save patcher, staged Apply/Restore replacement with rollback/recovery, and result-acknowledged session reload. APPLY, OPTIONS and PREVIEW are navigable from the top bar. Target build, Wii U filesystem behaviour, save-format invariants and physical-console validation remain incomplete. Keep an independent backup before any write-operation test.

The remaining increment sections in this document are historical development notes; where they differ from the current behaviour described above, the release-status and workflow sections at the top are authoritative.

## Current application workflow

- The application initialises the platform and UI, then enters its main lifecycle loop.
- It locates and reads the Home Menu save data.
- It parses the available title/folder/page information and builds a sorting plan.
- The UI exposes APPLY, OPTIONS and PREVIEW sections. APPLY invokes backup, Restore and Apply operations through the platform adapter.
- Preview navigation supports folder inspection and return to the originating page/selection.
- After A acknowledges an Apply/Restore result, whether success or failure, the same lifecycle loop reacquires the active save and rebuilds the session.
- Settings are persisted separately from the Home Menu save.

## Current controls and UI behaviour

- Top-bar order: OPTIONS → PREVIEW → APPLY.
- Shoulder navigation: OPTIONS (ZL → APPLY, ZR → PREVIEW); PREVIEW (ZL → OPTIONS, ZR → APPLY); APPLY (ZL → PREVIEW, ZR → OPTIONS). Entering PREVIEW from OPTIONS/APPLY starts generation immediately; A is not required.
- OPTIONS retains the v0.5 sorting controls, Excluded Pages field, and its original bottom-bar actions.
- A activates the focused APPLY item or virtual-keyboard key. Create Backup runs immediately; Apply and Restore use dedicated A/B decision screens. Result screens accept A only.
- Restore opens a newest-first mixed manual/automatic backup selector; D-Pad Up/Down selects an entry, A opens the decision screen, A confirms and B returns to the selector. Both success and failure results reload the session only after A acknowledgement.
- The Excluded Pages field uses a GamePad-controlled virtual keyboard and validates the expression through the existing parser.
- Preview title labels follow the selected sort field. Title ID remains visible even when metadata is missing; the Disc and vWii channels display `N/A` for Product Code because neither has a Product Code listed in the WiiUBrew title database.
- Titles without usable artwork use the bundled `resources/unknown.png`; the renderer no longer overlays misleading `T`, `D` or `W` letter symbols on generic tiles.

These controls are subject to change as the UI evolves. The application screen remains the authoritative reference for the current build.

## Metadata and artwork

- The coordinator requests metadata only for non-folder title IDs referenced by the parsed Home Menu save, rather than parsing every installed title. This includes native installed system applications when MCP returns their installation records.
- Native title metadata is read from installed `meta/meta.xml`, with ACP metadata as a fallback. Native artwork is loaded from the installation's `meta/iconTex.tga` path through SDL_image, trying both devoptab and CafeOS volume path forms.
- Homebrew on Menu (`0005000F`) entries are matched to SD executables using the same 32-bit path hash as the plugin. WUHB bundles are mounted with libwuhbutils so `meta/meta.ini` and available icon artwork can be read directly into the app's metadata catalog. The provider tries `meta/iconTex.tga`, common icon filename variants, and finally TV/GamePad artwork, with a 2 MiB safety cap on files read from bundles. SDL_image is followed by an in-memory TGA fallback decoder for supported uncompressed/RLE true-colour and grayscale variants. Standalone RPX entries use adjacent metadata/artwork when present. XML entities are decoded in native metadata, ACP fallbacks and Homebrew on Menu INI values.
- Mirrored NAND/USB records with the same ID preserve their special Disc/vWii type instead of being misclassified as Unknown. Disc (`00050010-10055000`, nominal ID, Product Code N/A) and the vWii launcher (`00050010-10004000`, OSv0/cafe2wii, no Product Code listed) use explicit fallback metadata. The Disc name is localized using the Wii U system language (Italian `Disco`). Both entries are sortable like ordinary titles; if a selected metadata field has no value, their Title ID provides a deterministic sorting fallback. Unknown records and nested folders remain fixed. Disc icon lookup tries the nominal system-title path and optical-volume metadata paths; Wii tries OSv0/OSv1 before legacy candidates, then falls back to `unknown.png`.
- The lock overlay is 60 × 60 logical pixels and anchored to the upper-right corner of the full title card, not the icon artwork.
- Folder and generic fallback artwork is bundled and prepared at build time.
- The folder-colour byte is read at offset `0x33` of the 56-byte folder metadata block, based on the current experimentally confirmed sample fixtures; treat this as a researched implementation detail, not a guarantee for every save revision.
- Bottom-bar button glyphs attempt to use the Wii U system shared font, with text fallbacks where glyphs are unavailable.

## Backup/restore implementation increment\n\n- `HomeMenuSaveBackupManager` copies the active save byte-for-byte; it does not reconstruct it from the parsed model. It validates the exact expected size and runs `ParseSave` before accepting a source or backup.\n- Manual and automatic backups use the requested timestamped names in one directory. Enumeration sorts newest-first; automatic retention removes only expired `autobackup_` files and never removes manual backups.\n- Restore reads, stages and verifies the selected backup before automatic retention can run. If Autobackup is enabled, it preserves the current active save in a new automatic backup before replacing it; a failed safety backup aborts Restore. It then preserves the current save as a rollback file during the rename sequence, validates the installed bytes, and rolls back on failure.\n- The core service is exposed through the Wii U platform adapter. The adapter selects the plugin-redirected active path when present and stores backups under the application SD directory. The UI is not yet connected.\n\n## Apply transaction implementation increment

- `HomeMenuSaveApplyTransaction` verifies that the active file still matches the exact byte sequence used to generate the preview. A stale preview is rejected before backup creation or file mutation.
- The candidate is built in memory and reparsed before an optional autobackup is created. It is then written to a sibling staging file, reread, compared byte-for-byte and reparsed.
- The original active save is renamed to `.apply.previous` before the candidate is committed. The installed file is reopened and verified; failures attempt to restore the original. The previous file is kept if cleanup fails.
- `RecoverInterruptedApply` restores the preserved original after an interrupted transaction. The Wii U save acquisition adapter invokes this routine on the resolved authoritative path before reading the save, so an interrupted transaction is rolled back before the session parses the file.
- These operations currently use host-tested C file/rename operations. Their behaviour and durability on Wii U devoptab/FAT storage are not verified.

## Important limitations

- The backup manager and Apply transaction are host-testable core services, not yet connected to the Wii U filesystem paths or UI. No APPLY action is available in the application yet.
- The Home Menu save format is only partially understood; unknown/opaque data must be preserved by any future parser work.
- The page/slot mapping and assumptions about the number of items per page have not been fully verified.
- MCP/ACP enumeration, SD WUHB bundle access through libwuhbutils, SDL_image memory decoding for extracted TGA/PNG artwork, optical-volume icon access, regional Wii Menu icon paths, system-font glyphs, and HOME → Continue lifecycle behaviour require testing on physical hardware. Host tests validate the provider's selection logic and metadata propagation only through mocks.
- A passing host test suite does not constitute hardware validation.

## Validation status for this snapshot

The source snapshot includes host tests and test doubles for selected platform/UI interfaces. The Wii U target cross-build and complete on-console validation were not performed in the environment in which the optimisation archive was prepared. Treat the first build on the maintainer's devkitPro installation as a required validation step.

## Publication note

`tests/fixtures/sample_menu_save.dat` is a local-only development fixture excluded by `.gitignore`. It is not committed to Git; the maintainer must retain a local copy to run the core integration test. See `docs/RELEASE_CHECKLIST.md`.


## Wii U backup platform integration increment

- Added explicit platform operations for manual backup, listing backups, Restore and Apply. They use the active path selected by the save locator, including the Homebrew On Wii U Menu redirected path when present.
- Backups are stored in `fs:/vol/external01/wiiu/apps/TitlesSorter/backups`; manual backup names and automatic backup retention continue to be managed by `HomeMenuSaveBackupManager`.
- Added startup recovery for interrupted Restore transactions, before Apply recovery and before parsing the active save. Recovery validates the preserved original and restores it byte-for-byte.
- These operations are service-level integration only: no UI control invokes them yet. Do not consider Apply or Restore user-accessible in this increment.
- Wii U devkitPPC compilation and hardware filesystem semantics remain unverified.


## APPLY / backup UI integration increment — 2026-11-05

- Added the APPLY top-bar section while preserving the v0.5 OPTIONS controls and PREVIEW grid/navigation. The top-bar order is OPTIONS → PREVIEW → APPLY; the group expands to the left while the ZR control remains at its prior X position.
- Added Autobackup (default Enabled), maximum autobackups (default 10, accepted range 1–100), manual backup, backup selection/Restore and Apply Previewed Layout rows.
- Maximum autobackup count uses the Wii U software keyboard; invalid values do not replace the last valid setting. Settings v2 persists both new values.
- Manual backup runs immediately. Restore uses a scrollable newest-first file selector and an A/B decision; when Autobackup is enabled, it creates a safety backup before replacement. Apply uses an A/B decision and the same Autobackup setting.
- Connected the UI requests to `WiiUHomeMenuSavePlatform`, refreshed the backup list after manual backup, and reloads the active save/session after successful Restore or Apply.
- A cached plan is considered current only while the sorting options and metadata reference still match. Changing sorting options disables Apply until a new preview has been generated. Backup settings do not invalidate a sorting plan.
- Host suite: **13/13 test executables passed** using GCC 14.2.0, C++20 and strict warnings. UI tests cover section navigation, autobackup defaults, numeric validation, manual backup confirmation, backup selection/Restore confirmation and Apply confirmation.
- Not run: devkitPPC/WUT target build or physical-console test. The current environment has no `DEVKITPRO`, `DEVKITPPC` or `powerpc-eabi-g++`. Do not treat host tests as proof of Wii U filesystem durability or save-format correctness.

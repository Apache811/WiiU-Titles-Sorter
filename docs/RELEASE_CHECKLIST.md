# v1.0.0 release checklist

This checklist applies to the v1.0.0 write/apply release. Complete every applicable item before publishing the source or binary. The v0.5.0 read-only release is historical.

## Repository hygiene and privacy

- [ ] Verify `tests/fixtures/sample_menu_save.dat` is ignored by Git (`git check-ignore -v tests/fixtures/sample_menu_save.dat`) and is absent from the staged changes. The local core integration test still requires the maintainer to keep this file locally.
- [ ] Review all files under `tests/fixtures/`, logs, screenshots, and documentation for console serials, title/account identifiers, personal paths, save data, or other private information.
- [ ] Confirm `.gitignore` excludes `build/`, compiler outputs, generated WUHB/RPX/ELF files, and local-only data.
- [x] No `LICENSE` file is included, per maintainer decision. Do not describe the project as open source or imply reuse rights.

## Source and tests

- [ ] Run `make tests` from a clean checkout using the documented host toolchain.
- [ ] Run `make resources` and verify the generated assets.
- [ ] Review the final diff and ensure the Makefiles, source artwork, font, metadata, and required build scripts are included.
- [ ] Record the exact Git commit hash used for the release.

## Wii U build and hardware smoke test

- [ ] Build from a clean checkout with the documented devkitPro/devkitPPC/WUT dependencies: `make clean && make`.
- [ ] Confirm `build/wiiu/release/TitlesSorter.wuhb` is generated.
- [ ] Install/run the WUHB on the target Wii U with the intended Aroma setup.
- [ ] Verify cold launch, initial save loading, APPLY/OPTIONS/PREVIEW switching, sorting, page wrap navigation, folder preview/navigation, metadata/artwork, and settings persistence.
- [ ] Verify manual backup creation, filename format, backup enumeration and backup-selector scrolling.
- [ ] Verify invalid autobackup retention input preserves the previous value and valid input persists across restart.
- [ ] Verify Restore selection is newest-first across manual and automatic backups, Restore confirmation, success/failure result screens, A-only acknowledgement, session reload after either outcome, and recovery from an interrupted Restore.
- [ ] Verify Restore with Autobackup enabled creates and verifies a byte-for-byte backup of the pre-Restore active save before replacement; verify a backup failure aborts Restore and leaves the active save unchanged.
- [ ] Verify Restore with Autobackup disabled proceeds without creating an automatic backup.
- [ ] Verify the selected automatic backup remains restorable when the subsequent retention pass removes its original directory entry.
- [ ] Verify Apply creates an autobackup when enabled, respects the retention limit, does not create one when disabled, and rejects a stale preview.
- [ ] Verify Apply/Restore rollback and startup recovery after deliberately simulated failures before using real user save data. Confirm result screens remain visible until A and reload occurs only after acknowledgement, on success and failure.
- [ ] Confirm the Wii U filesystem's rename/flush behaviour and test on a disposable/independently backed-up save before considering Apply safe.
- [ ] Verify returning through HOME → Continue/resume if that scenario is in scope for this build.
- [ ] Confirm that the HOME Menu layout is not modified by the application.
- [ ] Record the console environment, devkitPro package versions, commit hash, test outcome, and any known issues in `TEST_RESULTS.md`.

## GitHub release

- [ ] Push the reviewed source commit.
- [ ] Create and push the annotated tag `v1.0.0` only after the release gates above are satisfied.
- [ ] Create a GitHub Release from `v1.0.0`, with release notes describing backup/Restore/Apply behaviour and remaining limitations.
- [ ] Attach the tested `TitlesSorter.wuhb` only if the target build and physical-console test passed.
- [ ] Do not attach personal save files, local build folders, or unreviewed debug logs.

## Suggested v1.0.0 release description

**Titles Sorter v1.0.0 — Wii U Home Menu layout manager**

This release reads and previews the Wii U HOME Menu layout and provides manual/automatic backups, Restore and Apply operations. The save format and page/slot mapping remain experimental; publish only after the target build, filesystem semantics and physical-console release gates have passed. Application settings are stored separately from the Home Menu save.

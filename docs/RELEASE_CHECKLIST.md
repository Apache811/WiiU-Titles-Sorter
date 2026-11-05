# v0.5 release checklist

This checklist applies to the read-only v0.5 release. Complete each item before making the corresponding source or binary release public.

## Repository hygiene and privacy

- [ ] Keep the GitHub repository **Private** until all personal/sample data has been removed or replaced.
- [ ] Replace `tests/fixtures/sample_menu_save.dat` (derived from a user-provided save sample) with a synthetic fixture. Preserve equivalent parser/planner integration coverage; do not merely rename or repackage the original.
- [ ] Review all files under `tests/fixtures/`, logs, screenshots, and documentation for console serials, title/account identifiers, personal paths, save data, or other private information.
- [ ] Confirm `.gitignore` excludes `build/`, compiler outputs, generated WUHB/RPX/ELF files, and local-only data.
- [ ] Add a `LICENSE` file before describing the repository as open source or inviting reuse/contributions. No licence is currently selected.

## Source and tests

- [ ] Run `make tests` from a clean checkout using the documented host toolchain.
- [ ] Run `make resources` and verify the generated assets.
- [ ] Review the final diff and ensure the Makefiles, source artwork, font, metadata, and required build scripts are included.
- [ ] Record the exact Git commit hash used for the release.

## Wii U build and hardware smoke test

- [ ] Build from a clean checkout with the documented devkitPro/devkitPPC/WUT dependencies: `make clean && make`.
- [ ] Confirm `build/wiiu/release/TitlesSorter.wuhb` is generated.
- [ ] Install/run the WUHB on the target Wii U with the intended Aroma setup.
- [ ] Verify cold launch, initial save loading, OPTIONS/PREVIEW switching, sorting, page wrap navigation, folder preview/navigation, metadata/artwork, and settings persistence.
- [ ] Verify returning through HOME → Continue/resume if that scenario is in scope for this build.
- [ ] Confirm that the HOME Menu layout is not modified by the application.
- [ ] Record the console environment, devkitPro package versions, commit hash, test outcome, and any known issues in `TEST_RESULTS.md`.

## GitHub release

- [ ] Push the reviewed source commit.
- [ ] Create and push the annotated tag `v0.5.0` only after the release gates above are satisfied.
- [ ] Create a GitHub Release from `v0.5.0`, with release notes describing the read-only scope and known limitations.
- [ ] Attach the tested `TitlesSorter.wuhb` only if the target build and physical-console test passed.
- [ ] Do not attach personal save files, local build folders, or unreviewed debug logs.

## Suggested v0.5 release description

**Titles Sorter v0.5.0 — Read-only preview**

This release reads the Wii U HOME Menu layout, provides sorting options, and previews the proposed arrangement. It does not write, replace, or apply changes to the HOME Menu save. Application settings are stored separately. The save format and some platform-specific metadata/artwork paths remain under investigation; consult the README and known limitations before use.

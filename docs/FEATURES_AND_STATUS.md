# Features and implementation status

This document records the intended scope and current implementation boundaries. It should be updated when a feature is added, removed, or validated on hardware.

## Release status — v0.5 read-only

The v0.5 feature scope is considered complete for the read-only workflow. This is a source-level release candidate, not a claim that the current archive has already been built and validated on a physical Wii U. The host-side test results are recorded in `TEST_RESULTS.md`; target compilation and hardware validation remain release-maintainer steps.

The application does not write, replace, or apply changes to the Wii U HOME Menu save. Its own settings are stored separately. Do not describe the preview as a committed layout.

## Milestone scope

- **v0.5:** feature-complete read-only workflow: load and inspect the Home Menu, configure sorting, generate the preview, resolve available title metadata/artwork, and persist Titles Sorter settings separately. No Home Menu save bytes are written.
- **v1.0:** a future milestone for applying and saving the planned Home Menu layout. It is out of scope until the save structure, integrity requirements, and safe write/replace procedure are validated on hardware.

## Current application workflow

- The application initialises the platform and UI, then enters its main lifecycle loop.
- It locates and reads the Home Menu save data.
- It parses the available title/folder/page information and builds a sorting plan.
- The UI exposes sorting options and a preview of the planned layout.
- Preview navigation supports folder inspection and return to the originating page/selection.
- Settings are persisted separately from the Home Menu save.

## Current controls and UI behaviour

- ZL and ZR toggle between OPTIONS and PREVIEW.
- A selects/activates the focused item or virtual-keyboard key.
- B cancels virtual-keyboard entry or exits the currently opened folder preview; it does not act as a general Preview-to-Options shortcut.
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

## Important limitations

- No save-writing or save-replacement implementation is present.
- The Home Menu save format is only partially understood; unknown/opaque data must be preserved by any future parser work.
- The page/slot mapping and assumptions about the number of items per page have not been fully verified.
- MCP/ACP enumeration, SD WUHB bundle access through libwuhbutils, SDL_image memory decoding for extracted TGA/PNG artwork, optical-volume icon access, regional Wii Menu icon paths, system-font glyphs, and HOME → Continue lifecycle behaviour require testing on physical hardware. Host tests validate the provider's selection logic and metadata propagation only through mocks.
- A passing host test suite does not constitute hardware validation.

## Validation status for this snapshot

The source snapshot includes host tests and test doubles for selected platform/UI interfaces. The Wii U target cross-build and complete on-console validation were not performed in the environment in which the optimisation archive was prepared. Treat the first build on the maintainer's devkitPro installation as a required validation step.

## Publication note

The development archive contains `tests/fixtures/sample_menu_save.dat`, which was identified as derived from a user-provided save sample. It is not suitable for public distribution. Keep the initial GitHub repository private until this fixture has been replaced by a synthetic fixture (with equivalent integration coverage) or the test has been changed to generate synthetic input at runtime. See `docs/RELEASE_CHECKLIST.md`.

# Changelog

Notable project changes are recorded here. Version numbers follow the `MAJOR.MINOR.PATCH` format; v0.5.0 is the read-only milestone and does not include HOME Menu save writing.

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

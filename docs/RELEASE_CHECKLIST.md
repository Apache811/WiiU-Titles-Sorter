# v1.0.0 release record

Titles Sorter v1.0.0 is complete, validated and published. This file records the release decisions and repository-specific handling; it is not an outstanding pre-release checklist.

## Release scope

The published v1.0.0 includes the complete OPTIONS, PREVIEW and APPLY workflow; settings persistence; installed-title and Homebrew metadata/artwork; manual and automatic backups; mixed newest-first backup selection; Restore with optional pre-Restore autobackup; and staged Apply with verification and recovery handling.

## Repository hygiene and privacy

- `tests/fixtures/sample_menu_save.dat` is intentionally excluded by `.gitignore`. It is a local test input and is not uploaded to the repository.
- Review console dumps, save data, logs containing identifiers, personal paths and other private information before adding files to the repository.
- Build outputs and generated artifacts are managed by `.gitignore`; generated build files are not source inputs.
- No `LICENSE` file is included, by maintainer decision. Do not describe the project as open source or imply reuse rights.

## Release validation

The maintainer has completed the final validation and published v1.0.0. The development-stage entries in `TEST_RESULTS.md` and `CHANGELOG.md` are retained as historical records. Their statements that a particular build, test or hardware check had not yet been performed refer to those earlier snapshots and are not pending actions for the published release.

## Subsequent maintenance

For a future release:
- Record the source revision and release artifact together.
- Update `README.md`, `FEATURES_AND_STATUS.md`, `CHANGELOG.md` and `TEST_RESULTS.md` when shipped behaviour changes.
- Keep the local save fixture excluded from Git.
- Re-run the relevant host tests and the Wii U validation appropriate to the changed functionality.
- Update the version and release artifact consistently.

# Features and implementation

## Release status — v1.0.0

Titles Sorter v1.0.0 is the completed, validated and published release. This document describes the implementation included in that release. Historical development and test notes remain in `CHANGELOG.md` and `TEST_RESULTS.md`; their point-in-time limitations do not describe the final release state.

## Application workflow

1. The Wii U platform layer initializes the required system, storage, input and rendering services.
2. The application resolves the active Home Menu save location, performs recovery checks for interrupted write transactions, and reads the active save.
3. The save is parsed and the sorting session is created.
4. Installed-title and Homebrew on Menu metadata are loaded incrementally, one item per loading frame.
5. Persisted settings are applied to the created session.
6. The user can configure sorting in OPTIONS, inspect the generated arrangement in PREVIEW, and perform backup/Restore/Apply operations in APPLY.
7. After A acknowledges a Restore or Apply result, the application reloads the active save and metadata. This occurs after both successful and failed operations.

Application settings are stored separately from the Home Menu account save.

## Sorting and preview

- Sort fields: Title ID, Product Code, Long Name and Short Name.
- Folder placement: Folders First or Mixed Alphabetically.
- Optional sorting of folder contents.
- Excluded Home Menu pages are configured with the excluded-pages expression and validated by the existing parser.
- Preview generation begins immediately when PREVIEW is entered; no additional A confirmation is required.
- Preview supports page navigation, folder entry and return to the previous Home Menu page and selection.
- Disc and vWii channels participate in sorting. Unknown records and nested folders remain fixed according to the planner's rules.
- When a selected metadata field is unavailable, Title ID provides a deterministic sorting fallback.

## Title metadata and artwork

- Metadata is requested for title IDs referenced by the active Home Menu save rather than indiscriminately processing every installed title.
- Native installed-title metadata is read from installed title metadata files, with ACP fallbacks where available.
- Homebrew on Menu entries are matched to SD executables using the plugin-compatible path hash. WUHB metadata and artwork are read through libwuhbutils; RPX sidecar metadata/artwork is used when available.
- WUHB artwork lookup tries dedicated icon assets and then GamePad/DRC artwork. SDL_image is used where supported, with an in-memory TGA decoder fallback for supported TGA variants.
- XML entities are decoded in native metadata, ACP fallback strings and Homebrew on Menu INI values.
- Disc and vWii have special-title fallback metadata. Disc naming follows the Wii U system language where a translation is available. Product Code is shown as unavailable for these special channels.
- Missing artwork uses the bundled generic artwork. Fixed entries display the lock overlay.
- Folder artwork and runtime UI assets are prepared by the build-time resource pipeline.

## Settings

Settings are persisted in:

`fs:/vol/external01/wiiu/apps/TitlesSorter/settings.ini`

The versioned settings file stores sorting options, folder behaviour, excluded pages, Autobackup and maximum automatic backups. Version 1 settings remain readable and are upgraded to version 2 on a successful settings write. Defaults are used when the file is absent or rejected.

Autobackup defaults to Enabled. Maximum automatic backups defaults to 10 and accepts values from 1 to 100.

## Backup and Restore

Backups are stored in:

`fs:/vol/external01/wiiu/apps/TitlesSorter/backups`

- Manual and automatic backups preserve the active save byte-for-byte.
- Backup files are named with a timestamp.
- The Restore selector presents manual and automatic backups in one list, ordered newest first regardless of backup type.
- Automatic retention removes only automatic backups; manual backups are not deleted by retention.
- When Autobackup is enabled, Restore stages and verifies the selected backup, then creates an automatic backup of the currently active save before replacement.
- If the pre-Restore autobackup cannot be completed, Restore is aborted and the active save remains unchanged.
- The selected backup is staged before retention is applied, so retention cannot invalidate the in-progress Restore operation.
- Restore uses staged replacement, verification and recovery handling.

## Apply

- Apply is available only for a current preview plan.
- Before writing, the transaction verifies that the active save still matches the save used to generate the preview. A stale preview is rejected before mutation.
- The candidate save is built in memory and validated.
- When Autobackup is enabled, an automatic backup is created before Apply.
- The candidate is written to a sibling staging file, reread and verified before replacement.
- The previous active save is preserved during the replacement sequence. The installed result is verified, and failure handling attempts to restore the previous save.
- Startup recovery checks for interrupted Restore and Apply transactions before the active save is parsed.

## User interface

The top-bar order is OPTIONS → PREVIEW → APPLY.

| Current section | ZL | ZR |
|---|---|---|
| OPTIONS | APPLY | PREVIEW |
| PREVIEW | OPTIONS | APPLY |
| APPLY | PREVIEW | OPTIONS |

OPTIONS retains the sorting controls and original bottom-bar actions. APPLY provides Autobackup, maximum automatic backups, Create Backup, Restore Backup and Apply the Previewed Layout.

Create Backup executes immediately after A and displays a success/failure result with the path or error. Restore and Apply use dedicated decision screens and result screens. Result screens are acknowledged with A; B is ignored there. After acknowledgement, Restore and Apply both request a session reload, whether the operation succeeded or failed.

The Preview Required message uses the same result-screen presentation convention as the Create Backup result. Folder navigation preserves the originating selection.

## Build and tests

The repository contains separate resource preparation, Wii U application and host-test build pipelines. See `docs/BUILD_ENVIRONMENT.md` and `docs/BUILD_GUIDE.md`.

The host suite covers portable core logic and selected UI/platform adapters through test doubles. The release validation was completed by the maintainer; historical environment-specific test records are retained in `TEST_RESULTS.md`. Those historical records describe the individual development snapshots at the time they were produced and are not outstanding release tasks.

## Repository-specific notes

- `tests/fixtures/sample_menu_save.dat` is a local test fixture excluded by `.gitignore`; it is not part of the published repository.
- No `LICENSE` file is included. The repository does not declare an open-source licence or grant reuse rights.

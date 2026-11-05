# Titles Sorter

<p align="center">
  <img src="docs/banner.png" alt="Titles Sorter banner" width="100%" />
</p>

<p align="center">
  <img src="docs/icon.png" alt="Titles Sorter application icon" width="128" height="128" />
</p>

**Titles Sorter is a Wii U homebrew application for organising titles on the Wii U HOME Menu.**

The **v1.0.0 release is complete, validated and published**. It reads the active HOME Menu layout, builds and previews sorting plans, and provides settings persistence, manual and automatic backups, Restore, and Apply.

## Screenshots

<p align="center"><img src="docs/screenshot_00.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_01.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_02.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_03.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_04.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_05.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_06.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_07.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_08.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_09.jpg" alt="Titles Sorter screenshot" width="100%" /></p>
<p align="center"><img src="docs/screenshot_10.jpg" alt="Titles Sorter screenshot" width="100%" /></p>

## v1.0.0 features

- Reads the active Wii U HOME Menu save and presents installed titles, folders and supported special channels.
- Builds sorting plans using the configured sort field, folder placement, folder-content sorting and excluded-page expression.
- Provides OPTIONS, PREVIEW and APPLY sections. Entering PREVIEW generates the preview immediately.
- Supports browsing folder contents and returning to the originating Home Menu page and selection.
- Resolves installed-title metadata and artwork, plus Homebrew on Menu metadata and WUHB/RPX artwork where available. Missing artwork uses the bundled generic image; fixed entries display the lock overlay.
- Includes Disc and vWii channel handling, localized Disc naming where supported, and deterministic sorting fallbacks when Product Code metadata is unavailable.
- Persists application settings separately from the Home Menu save.
- Creates manual backups and optional automatic backups. Automatic retention applies only to automatic backups.
- Lists manual and automatic backups together, newest first.
- When Autobackup is enabled, creates a safety autobackup before both Apply and Restore. If that backup cannot be created, the corresponding operation is not carried out.
- Applies the previewed layout through a staged save replacement with verification and recovery handling.
- Restore and Apply show an operation result. After the user acknowledges the result, the application reloads the active save and title metadata, including after a failed operation.
- Includes a host-side test suite for portable core logic and selected UI/platform components.

## Build

The project has separate resource, Wii U application and host-test build pipelines, orchestrated by the top-level `Makefile`.

### Requirements

- **Wii U application:** devkitPro/devkitPPC, WUT, Wii U SDL2, SDL2_image, SDL2_ttf, libmocha and libwuhbutils, with WUHBUtilsModule available in the Aroma environment.
- **Resource preparation:** Python 3.10+ and Pillow.
- **Host tests:** MSYS2 with the UCRT64 GCC toolchain.

See:
- [Build environment](docs/BUILD_ENVIRONMENT.md)
- [Build guide](docs/BUILD_GUIDE.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)

### Quick commands

Run from the repository root in the configured MSYS2 shell.

```sh
# Prepare runtime artwork
make resources

# Build the Wii U application (resources, RPX and WUHB)
make

# Rebuild runtime artwork
make resources-rebuild

# Build and run host tests
make tests

# Build the read-only host save-planner utility
make dryrun

# Normalise repository timestamps
make normalize-timestamps

# Remove generated outputs
make clean
```

The Wii U build prepares runtime artwork under `build/assets/wiiu/`, compiles the application under `build/wiiu/`, and places the distributable WUHB under `build/wiiu/release/`.

## Backup and Restore behaviour

Backups are stored under the application data directory:

`fs:/vol/external01/wiiu/apps/TitlesSorter/backups`

Manual and automatic backups use timestamped filenames. The Restore selector combines both types and orders them by timestamp, newest first. Automatic retention removes only automatic backups; manual backups are not removed by that policy.

When Autobackup is enabled, Restore stages and verifies the selected backup, then creates an automatic backup of the currently active save before replacing it. If the safety backup fails, Restore is cancelled and the active save is left unchanged. The selected backup is staged before retention runs, so the operation can complete even if retention removes the selected backup's original file.

## Repository layout

```text
include/       C++ interfaces and internal headers
src/core/      Save parsing, sorting, patching, backup, transaction and settings logic
src/ui/        UI controller and SDL renderer
src/platform/  Wii U platform adapters and application entry point
src/tools/     Host-side save-planner utility
resources/     Source artwork
fonts/         Bundled font
meta/          WUHB icon and boot-screen assets
tests/         Host tests, mocks and fixtures
docs/          Architecture, UI, platform and build documentation
tools/         Build-time asset preparation and Python discovery scripts
build/         Generated build output (not tracked by Git)
```

## Safety and privacy

Titles Sorter modifies the active Home Menu save only when the user explicitly confirms Restore or Apply. Keep an independent copy of important save data before performing write operations. Backup and recovery operations are designed to preserve the original bytes and to verify staged/installed files before completing replacement.

Do not publish personal console data, save dumps, logs containing identifiers, or other private material in issues, pull requests, fixtures or release assets.

`tests/fixtures/sample_menu_save.dat` is a local test input and is intentionally excluded from Git by `.gitignore`. Keep the local fixture available when running the Core integration test. Verify the ignore rule with:

```sh
git check-ignore -v tests/fixtures/sample_menu_save.dat
```

## Release and project history

- **v0.5.0** was the read-only inspection and preview milestone.
- **v1.0.0** is the completed and published release, including Backup, Restore and Apply.

See [Features and implementation](docs/FEATURES_AND_STATUS.md), [UI contract](docs/UI_CONTRACT.md), [Changelog](CHANGELOG.md), and the [test and validation record](TEST_RESULTS.md).

## License

No `LICENSE` file is included. No licence terms are granted or implied by this repository.

## Support ❤️❤️❤️

If you find Titles Sorter useful and would like to support its development, or simply say thank you, please consider leaving a tip. 😊

<https://ko-fi.com/apache81>

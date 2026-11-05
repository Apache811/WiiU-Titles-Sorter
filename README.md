# Titles Sorter

<p align="center">
  <img src="docs/banner.png" alt="Titles Sorter banner" width="100%" />
</p>

<p align="center">
  <img src="docs/icon.png" alt="Titles Sorter application icon" width="128" height="128" />
</p>

**A Wii U homebrew project for inspecting and planning the organisation of titles on the Wii U HOME Menu.**

Titles Sorter is a Wii U homebrew application for inspecting the current HOME Menu layout, configuring sorting options, and previewing a proposed arrangement. The v0.5 release is deliberately read-only with respect to the HOME Menu save.

> [!WARNING]
> **Experimental software — use at your own risk.** Save parsing and sorting/planning are currently read-only. The project does not implement a save writer, and the Home Menu save format and page/slot mapping are not fully understood. A preview is not a guarantee that a proposed layout can safely be written to the console. Do not use this project to modify or replace your Home Menu save data.

## Screenshots

<p align="center">
  <img src="docs/screenshot_00.jpg" alt="Screenshot 00" width="100%" />
</p>

<p align="center">
  <img src="docs/screenshot_01.jpg" alt="Screenshot 01" width="100%" />
</p>

<p align="center">
  <img src="docs/screenshot_02.jpg" alt="Screenshot 02" width="100%" />
</p>

<p align="center">
  <img src="docs/screenshot_03.jpg" alt="Screenshot 03" width="100%" />
</p>

<p align="center">
  <img src="docs/screenshot_04.jpg" alt="Screenshot 04" width="100%" />
</p>

<p align="center">
  <img src="docs/screenshot_05.jpg" alt="Screenshot 05" width="100%" />
</p>

<p align="center">
  <img src="docs/screenshot_06.jpg" alt="Screenshot 06" width="100%" />
</p>

<p align="center">
  <img src="docs/screenshot_07.jpg" alt="Screenshot 07" width="100%" />
</p>

## Project status

| Area | Status |
|---|---|
| Home Menu save discovery and reading | Implemented; hardware validation required |
| Save parsing and sorting planner | Implemented; format understanding is incomplete |
| Sorting options and preview UI | Implemented; hardware validation required |
| Folder preview and folder-content planning | Implemented; hardware validation required |
| Wii U title metadata and artwork | Implemented; hardware validation required |
| Settings persistence | Implemented; hardware validation required |
| Save writing / applying a new layout | **Not implemented** |
| Full validation on a physical Wii U | Pending for this source snapshot |

The **v0.5 release scope** is feature-complete read-only inspection and preview. Version **v1.0** is reserved for a future, separately validated implementation that applies the planned layout to the HOME Menu save. No HOME Menu save writer, save replacement, or apply operation is included in v0.5. Application settings are persisted separately from the HOME Menu save.

## Features in this snapshot

- Reads and analyses the Wii U Home Menu save data without writing it.
- Builds a sorting plan and previews the planned Home Menu arrangement.
- Supports sorting options and excluded-page expressions.
- Provides navigation between Options and Preview using the Wii U GamePad shoulder buttons.
- Supports opening a folder in Preview and returning to the previous page/selection.
- Retrieves metadata for installed system titles and Homebrew on Menu entries. WUHB metadata/icons are read directly from SD bundles through libwuhbutils, with a TGA decoding fallback for bundle artwork; `resources/unknown.png` is the generic fallback and `resources/lock.png` marks fixed tiles. Disc and Wii entries use their nominal system Title IDs, localized channel names where available, and remain sortable like ordinary channels; neither has a Product Code listed in the WiiUBrew title database.
- Persists application settings.
- Includes a portable host-side test suite for core logic and selected platform/UI adapters.
- Prepares bundled artwork at build time, keeping high-resolution source images separate from runtime assets.

See [docs/FEATURES_AND_STATUS.md](docs/FEATURES_AND_STATUS.md) for implementation details and known limitations, and [CHANGELOG.md](CHANGELOG.md) for the v0.5.0 release notes.

## Screenshots

Screenshots from a verified hardware build will be added here. The source snapshot has passed its host-side tests, but the Wii U target build and physical-console smoke test must be completed by the release maintainer before publishing a binary release.

## Build at a glance

The project has three separated build pipelines, orchestrated by the top-level `Makefile`:

- **Resource import:** Python 3.10+ and Pillow; `makeresources.mk` discovers a usable interpreter automatically, including Windows Python installations visible to MSYS2.
- **Wii U application:** devkitPro, devkitPPC, WUT, Wii U SDL2 port libraries, libmocha and libwuhbutils; the Aroma environment must also provide WUHBUtilsModule. `makeapp.mk` consumes generated resources and does not invoke Python.
- **Windows host tests:** MSYS2 with the UCRT64 GCC toolchain. Wii U SDK libraries are replaced by test doubles where applicable.

The commands below are a quick reference. Future-dated file timestamps are normalised automatically before resource import, Wii U compilation and host tests; `make normalize-timestamps` can also be run manually. For a clean machine setup, package details and troubleshooting, follow the dedicated guides:

- [Build environment setup](docs/BUILD_ENVIRONMENT.md)
- [Build and test guide](docs/BUILD_GUIDE.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)

### Quick commands

Run commands from the repository root in an MSYS2 shell configured for the relevant toolchain.

```sh
# Import/optimise resources only
make resources

# Build the Wii U application (resources first, then RPX and WUHB)
make

# Force a fresh resource import
make resources-rebuild

# Build and run the host-side test suite
make tests

# Build the read-only host save-planner utility
make dryrun

# Repair future-dated files manually (also runs automatically before builds)
make normalize-timestamps

# Remove generated outputs
make clean
```

The Wii U build first runs `makeresources.mk` to generate runtime artwork under `build/assets/wiiu/`, then runs `makeapp.mk` to compile the application. Intermediate files are placed under `build/wiiu/`, and the distributable WUHB is copied to `build/wiiu/release/`.

### Build-time artwork processing

Original artwork is kept in `resources/`. During the Wii U build, `tools/prepare_assets.py` uses Python 3 and Pillow to generate the runtime PNG assets in `build/assets/wiiu/`. Folder artwork is resized to the runtime dimensions before being embedded; `unknown.png` is prepared at 256×256 and `lock.png` at 48×48, while ZL/ZR artwork retains its source dimensions. The application loads the generated assets and does not resize folder artwork at runtime. See the [build guide](docs/BUILD_GUIDE.md#build-time-asset-pipeline).

## Repository layout

```text
include/       Public/internal C++ headers
src/core/      Portable save parsing, sorting, session and settings logic
src/ui/        UI controller and SDL renderer
src/platform/  Wii U platform adapters and application entry point
src/tools/     Host-only read-only save-planner utility
resources/     High-resolution source artwork
fonts/         Bundled font asset
meta/          WUHB icon and boot-screen assets
tests/         Host tests, mocks and test fixtures
docs/          Architecture, integration and build documentation
tools/         Build-time asset preparation and Python discovery scripts
Makefile       Top-level build dispatcher
makeresources.mk Resource import and optimisation pipeline
makeapp.mk     Wii U application compilation pipeline
maketests.mk   Windows host-test compilation pipeline
build/         Generated build output (not tracked by Git)
```

## Safety and privacy

- The current implementation does not write or modify the Home Menu save.
- Do not experiment with save replacement or manual editing based on the preview.
- Do not publish personal console data, dumps, save files, logs containing identifiers, or other user-provided material in issues, pull requests, test fixtures or releases.
- **PUBLICATION BLOCKER:** `tests/fixtures/sample_menu_save.dat` in this development snapshot was identified as derived from a user-provided sample. Do not push this file to a public repository or include it in a public release. Before making the repository public, replace it with a newly generated synthetic fixture and retain equivalent parser/planner integration coverage, or redesign that test to generate its input at runtime. Until that work is completed and reviewed, create the GitHub repository as **Private** and do not publish source archives containing this fixture.

## Release scope and versioning

- `v0.5.0` is the read-only milestone: inspection, sorting configuration, preview, metadata/artwork lookup, and separate application-settings persistence.
- `v1.0.0` is reserved for a future release that can safely apply a layout. It must not be implied or advertised by the v0.5 UI or documentation.
- A Git tag should identify each source release. Attach the matching `.wuhb` only after a successful devkitPPC/WUT build and a physical-console smoke test.
- See [Release checklist](docs/RELEASE_CHECKLIST.md) and [GitHub publishing guide](docs/GITHUB_PUBLISHING.md).

## Contributing

Bug reports and focused contributions are welcome. Please include:

- The exact source revision and build command.
- Whether the issue occurs in host tests, in the Wii U build, or on physical hardware.
- Relevant compiler output and a minimal reproduction.
- For hardware issues, console environment details (for example Aroma version and whether the issue occurs after a cold launch or HOME-menu resume).

**Never attach a personal Home Menu save dump.** If a report requires save data, provide a fully synthetic and reproducible fixture instead.

## License

No licence has been selected for this repository yet. Until a licence is added, the source is not granted an open-source licence and normal copyright restrictions apply. Add a `LICENSE` file before inviting external reuse or accepting contributions under explicit licensing terms.

## Support ❤️❤️❤️
If you find Titles Info useful and would like to support its future development, or simply say thank you, please consider leaving a tip. 😊
<https://ko-fi.com/apache81>

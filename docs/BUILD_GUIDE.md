# Build and test guide

Run all commands from the repository root in the MSYS2 MSYS shell. See [BUILD_ENVIRONMENT.md](BUILD_ENVIRONMENT.md) for installation and dependency setup.

## Build the Wii U application

```sh
make
```

The top-level build is intentionally split into two sequential stages:

1. `makeresources.mk` imports and optimises the source artwork into `build/assets/wiiu/`.
2. `makeapp.mk` validates the generated files, embeds them and the font through WUT's `bin2o` rules, compiles and links the RPX using WUT's linker specifications, produces the WUHB and copies it to the release directory.

`make` invokes the resource stage first and starts the application stage only after resource import succeeds. `makeapp.mk` no longer performs image conversion or calls Python.

Expected outputs:

| Output | Location |
|---|---|
| Generated runtime PNG assets | `build/assets/wiiu/` |
| Wii U objects and intermediates | `build/wiiu/` |
| RPX | `build/wiiu/TitlesSorter.rpx` |
| WUHB | `build/wiiu/TitlesSorter.wuhb` |
| Release copy | `build/wiiu/release/TitlesSorter.wuhb` |

The exact intermediate files are implementation details and may change with the WUT build rules.

## Import or rebuild resources independently

```sh
make resources           # Incremental import; skips processing when inputs are unchanged
make resources-rebuild   # Force a complete resource re-import
make cleanresources      # Remove only generated runtime resources
```

Resource import is owned by `makeresources.mk`. It resolves Python 3 + Pillow through `tools/resolve_python.sh`, then runs `tools/prepare_assets.py`. The original images in `resources/` are never modified. To select a particular Windows interpreter:

```sh
make resources ASSET_PYTHON="/c/Users/YourUser/AppData/Local/Programs/Python/Python313/python.exe"
```

The `ASSET_PYTHON` variable is also forwarded by the normal `make` application build.

## Run the host tests

```sh
make tests
```

This builds and runs the host-side test executables with UCRT64 GCC. The host build uses strict warnings (`-Wall -Wextra -Wpedantic -Wconversion -Werror`) and substitutes test doubles for selected SDL/WUT interfaces. It is intended to test portable logic and adapter behaviour, not emulate the Wii U runtime.

Useful variants:

```sh
make tests-rebuild
make tests-sanitize
```

`tests-sanitize` uses AddressSanitizer and UndefinedBehaviorSanitizer where supported by the host toolchain.

## Build the read-only dry-run utility

```sh
make dryrun
```

This builds a host executable that analyses a supplied save file and prints the planner result. It is not part of the Wii U application and does not write the save. Run it only against data you are authorised to inspect. Do not publish personal save files or its output if they contain identifying information.

## Clean and rebuild

```sh
make cleanapp      # remove Wii U application objects and binaries only
make cleanresources # remove generated Wii U runtime assets
make cleantests   # remove host-test output
make clean        # remove the complete build directory
make rebuild      # clean and rebuild Wii U application
make tests-rebuild
make normalize-timestamps # repair future-dated files (also runs automatically before builds)
```

## Build-time asset pipeline

Source artwork is stored under `resources/`; generated assets are stored under `build/assets/wiiu/` and must not be committed.

The `tools/prepare_assets.py` script currently processes the following assets:

- `folder_blue.png`, `folder_green.png`, `folder_yellow.png`, `folder_orange.png`, `folder_red.png`, `folder_rose.png`, `folder_purple.png`, `folder_grey.png`
- `ZL.png` and `ZR.png`
- `unknown.png`
- `lock.png`

Folder artwork is resized to 256 × 235 pixels, `unknown.png` to 256 × 256, and the transparent `lock.png` overlay to 48 × 48 with Pillow's high-quality Lanczos resampling. ZL/ZR artwork retains its source dimensions. The script writes optimised PNG files while preserving transparency. The Wii U renderer loads the generated folder and fallback artwork directly; it does not resize these textures at runtime.

Before Make evaluates build dependencies, the build entry points run `tools/normalize_timestamps.sh`. It finds files whose modification time is ahead of the current system clock (excluding `.git/`) and sets only those timestamps to the current time. This prevents archive extraction or clock skew from leaving source files in the future and causing misleading `Clock skew detected` warnings or stale incremental-build decisions. It does not rewrite file contents. The system clock itself must still be correct; timestamp normalisation cannot compensate for an incorrectly configured Windows clock. You can run `make normalize-timestamps` manually to apply the same repair.

`makeresources.mk` tracks the source artwork, preparation script and Python resolver through a stamp file. If the inputs are unchanged, Make reuses the prepared assets. To force a clean asset rebuild, use `make resources-rebuild`; to rebuild the complete application from clean Wii U objects and freshly imported resources, use `make rebuild`.

Do not edit files under `build/assets/wiiu/` by hand. Change the source artwork under `resources/` instead.

## Reproducibility notes

- Record the devkitPro package versions used for a release.
- Record the commit hash used to build the WUHB.
- Run host tests and a Wii U cross-build before publishing a release.
- Perform a physical-console smoke test; host tests cannot validate platform APIs, title metadata retrieval, controller glyph availability, or HOME-menu resume behaviour.
- Keep the original high-resolution source artwork in version control; generated build outputs are ignored by Git.

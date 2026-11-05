# Troubleshooting

## `DEVKITPRO is not set` or `DEVKITPPC is not set`

Open the MSYS2 shell in which devkitPro is configured and check:

```sh
printf '%s\n' "$DEVKITPRO" "$DEVKITPPC"
```

Set the variables to the actual installation paths for your machine. Do not copy a path from another Windows installation without checking it.

## `powerpc-eabi-g++` is missing or reports the wrong target

The Wii U Makefile intentionally uses `$(DEVKITPPC)/bin/powerpc-eabi-g++`, not the host UCRT64 compiler. Verify:

```sh
"$DEVKITPPC/bin/powerpc-eabi-g++" -dumpmachine
```

Expected target: `powerpc-eabi`.

## WUT rules or headers are missing

The build includes `$(DEVKITPRO)/wut/share/wut_rules`. Verify that WUT is installed in the same devkitPro root referenced by `DEVKITPRO` and that the Wii U development environment is selected in the package manager.

## SDL2, SDL_image, SDL_ttf or Mocha headers/libraries are missing

Check that the Wii U versions of the libraries are installed in the Wii U portlibs/WUT environment. A desktop Windows or Linux SDL installation cannot satisfy a Wii U cross-build. Search the configured devkitPro package repositories and compare the installed packages with the dependencies listed in [BUILD_ENVIRONMENT.md](BUILD_ENVIRONMENT.md).

## Python or Pillow is missing

The resource importer does not assume that `python` is available in the MSYS2 `PATH`. It searches MSYS2 Python installations and then Windows Python installations exposed through `where.exe`.

First, close and reopen MSYS2 if Python was installed or added to Windows `PATH` while the terminal was already open. Then check:

```sh
where.exe python.exe
make resources
```

If automatic discovery still fails, get the executable path in PowerShell:

```powershell
(Get-Command python.exe).Source
python -m pip show Pillow
```

Convert the Windows path to an MSYS path with `cygpath -u` if needed, then pass the exact interpreter to Make:

```sh
make resources ASSET_PYTHON="/c/Users/YourUser/AppData/Local/Programs/Python/Python313/python.exe"
make ASSET_PYTHON="/c/Users/YourUser/AppData/Local/Programs/Python/Python313/python.exe"
```

The first command imports resources only. The second performs the same import and then builds the Wii U application. Pillow must be installed into the exact Python interpreter selected by `ASSET_PYTHON`.

## Make reports files with modification times in the future

A warning such as `File '...' has modification time ... in the future` or `Clock skew detected` means the source file timestamps are ahead of the clock seen by MSYS2. Check that Windows date/time synchronisation is correct. Then, from the repository root, run:

```sh
make fix-timestamps
```

This target updates only repository files (excluding `.git/` and `build/`) whose timestamps are later than the current system time. It changes modification times, not file contents. The next build may recompile affected files once.

## A generated image is missing

The generated images live under `build/assets/wiiu/`; do not create or edit them manually. Re-run the resource importer:

```sh
make resources-rebuild
make
```

Check that all ten expected source PNGs exist under `resources/` and that Pillow can open them.

## Host tests cannot find `g++.exe`

Install the UCRT64 GCC package and verify the compiler path:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
/ucrt64/bin/g++.exe --version
make tests HOST_CXX=/ucrt64/bin/g++.exe
```

## Host tests pass but the application fails on Wii U

Host tests use mocks for selected platform interfaces and cannot validate the real Wii U services or graphics stack. Capture the complete build output, note the devkitPro/WUT package versions, and isolate whether the failure occurs during startup, save acquisition, metadata loading, rendering, controller input, or HOME-menu resume. Do not infer that the console toolchain is broken solely from a runtime hang; first compare the startup path and platform calls with a known-good homebrew.

## Before reporting a bug

Include the commit hash, build command, compiler/toolchain versions, complete relevant log, and whether the issue reproduces in host tests or on physical hardware. Never attach a personal Home Menu save dump; use a synthetic fixture.

# Build environment setup

This guide describes the toolchains required by the current Titles Sorter source snapshot. The Wii U target and Windows host tests are separate builds; installing the host compiler does not provide the Wii U SDK, and vice versa.

## 1. Windows and MSYS2

The Makefiles are intended to be run from an MSYS2 shell. Install MSYS2 from <https://www.msys2.org/> and keep its package database updated:

```sh
pacman -Syu
```

If the terminal asks you to close it, close it, reopen the appropriate MSYS2 shell, and complete the update before installing packages.

Use the **MSYS2 MSYS** shell for the project commands. The host-test Makefile explicitly adds `/ucrt64/bin` to `PATH` and invokes the UCRT64 compiler. The Wii U build invokes the devkitPPC compiler by its absolute path.

## 2. Wii U toolchain

Install devkitPro using the official instructions: <https://devkitpro.org/wiki/Getting_Started>. The devkitPro installation must include the Wii U development environment and devkitPPC.

The build expects these environment variables:

- `DEVKITPRO`: devkitPro installation root (commonly `/opt/devkitpro` in MSYS2 installations; use the actual path on your machine).
- `DEVKITPPC`: devkitPPC toolchain root, normally `$DEVKITPRO/devkitPPC`.

Check them in the MSYS2 shell:

```sh
printf 'DEVKITPRO=%s\n' "$DEVKITPRO"
printf 'DEVKITPPC=%s\n' "$DEVKITPPC"
test -x "$DEVKITPPC/bin/powerpc-eabi-g++" && echo 'devkitPPC compiler found'
```

The Makefile verifies that `powerpc-eabi-g++ -dumpmachine` reports `powerpc-eabi` before building the application.

### Required Wii U SDK components

The current Makefile includes WUT's `wut_rules` and links against WUT, Mocha and the Wii U SDL2 port. The source includes WUT interfaces from `coreinit`, `nn`, `vpad` and `whb`, plus Mocha's `mocha/mocha.h`.

Install the core Wii U packages through devkitPro pacman:

```sh
pacman -Syu --needed devkitPPC wut wut-tools wiiu-sdl2 wiiu-sdl2_image wiiu-sdl2_ttf
```

The SDL2 image/font packages pull in their declared dependencies, including the relevant Wii U/PowerPC portlibs. If pacman reports that a package is unavailable, update the package database and check the configured devkitPro repositories before trying to build a library manually:

```sh
pacman -Syu
pacman -Ss wiiu-sdl2
pacman -Ss wut
```

Package references: [Wii U homebrew development guide](https://www.wiiubrew.org/wiki/Homebrew_development_guide) and [devkitPro WUT packages](https://github.com/devkitPro/wut-packages).

At minimum, the environment must provide:

| Component | Why it is required |
|---|---|
| devkitPPC | `powerpc-eabi-gcc` / `powerpc-eabi-g++` cross compiler and binutils |
| WUT | Wii U headers, libraries, `wut_rules`, RPX/WUHB build integration |
| SDL2 Wii U port | Window, input and rendering APIs |
| SDL2_image Wii U port | PNG and installed title artwork loading |
| SDL2_ttf Wii U port | Font rendering |
| libmocha | Mocha APIs used by Wii U platform integration |
| libwuhbutils + WUHBUtilsModule | Read metadata and icon files from WUHB bundles on SD |
| WUT/portlibs transitive libraries | Image, font and compression dependencies linked by the Makefile |

The link line in `makeapp.mk` currently names SDL2_image, WebP, JPEG, SDL2_ttf, FreeType, HarfBuzz, PNG, bzip2, Brotli, SDL2, Mocha, zlib and WUT libraries. Install the matching Wii U portlibs/development packages from the configured devkitPro repositories so their headers and libraries are available under the Wii U portlibs/WUT roots.

### libwuhbutils and WUHBUtilsModule

Homebrew metadata and artwork are extracted from SD-resident WUHB bundles through
[libwuhbutils](https://github.com/wiiu-env/libwuhbutils). Its development library must
be installed under `$(DEVKITPRO)/wums`, and the matching `WUHBUtilsModule` must be
present under the Aroma environment's `modules` directory at runtime.

If the library is missing, build it in the configured MSYS2/devkitPro shell:

```sh
git clone https://github.com/wiiu-env/libwuhbutils.git
cd libwuhbutils
make install
```

The Wii U build checks for `wuhb_utils/utils.h` and `libwuhbutils.a` and stops with an
actionable error if either is missing. See the upstream README for any changed
toolchain-specific requirements.

### libmocha

The current source requires libmocha headers and library. The upstream project documents `make install` as its installation method and requires WUT: [wiiu-env/libmocha](https://github.com/wiiu-env/libmocha).

If libmocha is not already available in your devkitPro installation, build it from its upstream repository in the configured MSYS2/devkitPro environment:

```sh
git clone https://github.com/wiiu-env/libmocha.git
cd libmocha
make install
```

Check the upstream README and build output if its current instructions differ. Do not copy a host build of Mocha into the Wii U portlibs. The application also requires a compatible MochaPayload environment at runtime; see the [libmocha project notes](https://github.com/wiiu-env/libmocha) and your homebrew environment documentation.

## 3. Host test compiler (Windows)

The host tests use the UCRT64 GCC compiler and test doubles for Wii U/SDL interfaces where needed. Install it with:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc
```

The default compiler path is `/ucrt64/bin/g++.exe`. To override it:

```sh
make tests HOST_CXX=/ucrt64/bin/g++.exe
```

The test build does not require devkitPPC, WUT, or Wii U portlibs.

## 4. Python asset tooling

The resource pipeline requires Python 3.10 or newer and Pillow. It invokes Python from `makeresources.mk`; `makeapp.mk` does not invoke Python. The top-level Makefile defaults to `ASSET_PYTHON=auto` and uses `tools/resolve_python.sh` to find an interpreter that can actually import `PIL.Image`.

The resolver checks, in order:

1. `/ucrt64/bin/python.exe` and `/mingw64/bin/python.exe` (if installed in MSYS2).
2. `python3` and `python` visible in the MSYS2 shell.
3. Windows `python.exe` installations discoverable through `where.exe`, converting their paths with `cygpath` and ignoring the Windows Store execution alias.
4. The Windows Python Launcher (`py -3`), when available.

This means Python and Pillow installed for Windows from PowerShell can be used directly; they do not need to be reinstalled inside MSYS2, provided the Windows Python installation is on the Windows `PATH` inherited by the MSYS2 process. If you changed `PATH` during installation, close all MSYS2 terminals and open a new one before building.

### Verify the Windows Python installation

In PowerShell, check the interpreter and Pillow in the same environment:

```powershell
python --version
python -m pip show Pillow
(Get-Command python.exe).Source
```

In a newly opened MSYS2 MSYS shell, check that Windows exposes the interpreter:

```sh
where.exe python.exe
```

Then let the project resolve it automatically and import the resources:

```sh
make resources
```

The resolver validates Python 3.10+ and the Pillow import before running the asset script. It does not rely on the generic `python` command being present in MSYS2's own `PATH`.

### Select an interpreter explicitly

If automatic discovery cannot find the installation, copy the path returned by `(Get-Command python.exe).Source` in PowerShell, convert it with `cygpath` if necessary, and pass it to Make. For example:

```sh
make resources ASSET_PYTHON="/c/Users/YourUser/AppData/Local/Programs/Python/Python313/python.exe"
make ASSET_PYTHON="/c/Users/YourUser/AppData/Local/Programs/Python/Python313/python.exe"
```

The second command imports the resources first and then builds the Wii U application. Paths containing spaces are supported when quoted as shown.

If you prefer to install a Python environment inside MSYS2 instead, install a Python 3 interpreter and Pillow package from the matching MSYS2 repository, then confirm that `python -c "from PIL import Image"` succeeds in the shell. Do not install Pillow into one interpreter and point `ASSET_PYTHON` at another.

The resource build fails early with a clear diagnostic if no Python 3 interpreter with Pillow is available.

## 5. Verify the complete environment

From the repository root:

```sh
make help
make tests
make
```

`make tests` validates the host-side test suite. `make` validates the Wii U cross-build. A successful host test run does **not** prove that the Wii U target compiles or runs correctly on hardware.


## File timestamp normalisation

The Makefiles automatically run `tools/normalize_timestamps.sh` before resource import, Wii U application compilation and host-test builds. The script uses GNU `find` and `touch` supplied by the MSYS2 environment to detect files newer than the current system time and reset only those modification timestamps. It excludes `.git/` but includes generated files under `build/`, since future-dated object files or resource stamps can also confuse incremental builds.

You can invoke the same procedure manually from the repository root:

```bash
make normalize-timestamps
```

This changes timestamps only; it does not modify file contents. Ensure Windows automatic date/time synchronisation is enabled. If the system clock is wrong, correct it in Windows first rather than relying on timestamp normalisation.


The Python resolver enumerates all registered Windows Python installations with `py -0p`, rather than checking only the launcher's default interpreter. This allows it to select an older registered Python version if that is the one with Pillow installed.

# Wii U read-only save platform adapter

`src/platform/wiiu/WiiUHomeMenuSavePlatform.cpp` is the hardware-facing adapter for
`WiiUHomeMenuSaveReader`. It is deliberately excluded from normal desktop builds.

## Startup contract

The application must call `WHBProcInit()` before `WiiUHomeMenuSavePlatform::ReadActiveSave()`.
The adapter relies on the Homebrew environment's `fs:/vol/external01` mount for
the Homebrew On Wii U Menu plugin cache. It mounts only MLC through Mocha; USB
is not mounted or accessed.

## Acquisition sequence

1. Resolve the Wii U Menu title ID from `sysapp.rpl` using
   `_SYSGetSystemApplicationTitleId(0)`.
2. Initialize ACT, read the active slot and its persistent ID, then finalize ACT.
3. Read `code_id` and `serial_id` through MCP. The MCP settings buffer is
   explicitly 0x40-aligned, matching the working Menu Sort Aroma integration.
4. Initialize Mocha and mount `storage_mlc` at `/vol/storage_mlc01`.
5. Probe the plugin redirected path first, then use the portable reader's
   fail-closed precedence policy.
6. Open the selected file with `rb` only, verify its size and read exactly
   `0x1FC44` bytes. No write-capable file mode is used.
7. Unmount MLC when the adapter returns.

## Build integration

The adapter is included in the Wii U source set owned by `makeapp.mk`. The
current project snapshot does not yet have the Wii U application entry point,
so `make` deliberately stops before attempting to link an incomplete RPX/WUHB.
Once the lifecycle/main loop and WUHB assets are added, build it from the MSYS2
shell configured with devkitPPC, WUT and Mocha using `make`.

## Important verification boundary

The platform source was authored against the API usage in the supplied,
working `menu_sort-aroma-main` reference source (ACT, MCP, Mocha and dynamic
`sysapp.rpl` lookup). This development environment does not contain the Wii U
SDK headers, libraries, or `powerpc-eabi-g++`; therefore this source has **not**
been compiled against the actual installed WUT/Mocha versions and has not been
run on hardware. The portable reader and all host tests are independently
compiled and tested. The first devkitPPC build is the next required validation.

No save parser is invoked here, and no save file is created, changed, truncated,
renamed, deleted, or copied. This adapter is strictly read-only.

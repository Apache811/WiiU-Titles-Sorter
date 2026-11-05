# Wii U Home Menu save platform adapter

`src/platform/wiiu/WiiUHomeMenuSavePlatform.cpp` is the hardware-facing adapter for `WiiUHomeMenuSaveReader` and the backup/apply core services.

## Startup contract

The application must call `WHBProcInit()` before `WiiUHomeMenuSavePlatform::ReadActiveSave()`.
The adapter relies on the Homebrew environment's `fs:/vol/external01` mount for the Homebrew On Wii U Menu plugin cache and application data. It mounts only MLC through Mocha; USB is not mounted or accessed.

## Acquisition sequence

1. Resolve the Wii U Menu title ID from `sysapp.rpl` using `_SYSGetSystemApplicationTitleId(0)`.
2. Initialize ACT, read the active slot and its persistent ID, then finalize ACT.
3. Read `code_id` and `serial_id` through MCP. The MCP settings buffer is explicitly 0x40-aligned, matching the working Menu Sort Aroma integration.
4. Initialize Mocha and mount `storage_mlc` at `/vol/storage_mlc01`.
5. Probe the plugin redirected path first, then use the portable reader's fail-closed precedence policy.
6. Recover an interrupted Restore, then an interrupted Apply, on the resolved authoritative path before parsing it.
7. Read exactly `0x1FC44` bytes and reject unsupported sizes.
8. Keep MLC mounted for the application lifetime (the owner is `main()`).

## Build integration

The adapter and application entry point are included in the Wii U source set owned by `makeapp.mk`. The build discovers `.cpp` files under `src/core`, `src/ui`, `src/ui/sdl` and `src/platform/wiiu`.

Build from the configured MSYS2 shell with devkitPPC, WUT, Mocha, SDL2 ports and libwuhbutils installed. The main `Makefile` was not changed as part of the UI integration.

## Explicit write operations

The platform adapter exposes methods for manual backup, backup enumeration, Restore and Apply. Backups are placed under `fs:/vol/external01/wiiu/apps/TitlesSorter/backups`, in the same application folder as `settings.ini`.

- Manual backup copies the current active save byte-for-byte.
- Apply uses the active path selected by the save locator and the current preview plan. Autobackup is enabled by default and retention defaults to 10; only old autobackups are deleted.
- When Autobackup is enabled, Restore first stages and verifies the selected backup, then creates a byte-for-byte automatic backup of the currently active save. Restore is aborted if the safety autobackup cannot be completed. When disabled, Restore proceeds without creating an automatic backup.
- Apply and Restore use staged replacement and recovery files. Startup checks those recovery files before parsing the save.
- The UI now invokes these methods through explicit confirmation actions and reloads the session after the user acknowledges either a successful or failed Apply/Restore result with A.

## v1.0.0 release

The Wii U platform adapter, save acquisition, backup, Restore, Apply and recovery paths are part of the published v1.0.0 application. The maintainer completed the final build and console validation for the release.

The implementation deliberately fails closed on unsupported save sizes and verifies staged data before replacement. Keep independent backups of valuable save data as a general data-safety practice.

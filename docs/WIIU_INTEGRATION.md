# Wii U integration notes

## Metadata loading

The Wii U metadata provider is split into two parts:

- `WiiUMetadataProvider`: portable XML parsing, language mapping and in-memory catalog insertion.
- `WiiUInstalledMetadataProvider`: MCP enumeration and installed-title filesystem access, implemented under `src/platform/wiiu/`.

The adapter uses `MCP_TitleListByDeviceType(MCP_DEVICE_TYPE_AUTO, ...)`, not `MCP_TitleList`. The Homebrew on Menu plugin hooks `MCP_TitleList` and appends synthetic records with IDs in the `0005000F` family; the ordinary title count may not account for those appended records, so using that API with a count-sized buffer is unsafe. The integrated coordinator supplies the provider with the distinct non-folder title IDs referenced by the parsed save. The provider filters MCP's native title list against those IDs, so installed system applications such as Mii Maker are eligible for metadata lookup without parsing unrelated installed titles. Synthetic Homebrew metadata is resolved separately through `AddHomebrewTitleMetadata(titleId, error)`. Because Homebrew on Menu's ACP/FS hooks target the Wii U Menu process, this application must not rely on those hooks being active in its own process. The provider scans `SD:/wiiu/apps` and one subdirectory level, reproduces the plugin's `hash_string(relativePath)` algorithm (`hash = 37 * hash + byte`), and matches the resulting lower 32-bit ID. WUHB entries are read through libwuhbutils (`meta/meta.ini`, dedicated icon candidates, and GamePad/DRC artwork as a fallback); RPX entries use adjacent metadata/artwork when available. ACP and the virtual MLC icon path are fallback paths only. Only IDs actually present in the save are resolved.

## Runtime integration

`WiiUInstalledMetadataProvider` is driven incrementally by the application lifecycle. `LoadStep` processes at most one title per loading frame. Missing or malformed optional metadata is handled as an item-level issue so the remaining catalog can continue loading; failure to enumerate installed titles is reported as a metadata-loading error.

The provider is integrated with the coordinator and the UI preview pipeline. It is not a standalone sample integration: the application entry point owns its lifetime and advances it from the main lifecycle loop.

## Target build and release

The Wii U application source set is discovered by `makeapp.mk`. The real target build uses WUT headers and libraries; `tests/wut_mock` is reserved for host-side compile-shape checks and is not part of the target.

The published v1.0.0 includes the Wii U metadata provider, Homebrew on Menu/WUHB handling, the platform save adapter and the complete Backup/Restore/Apply workflow. The maintainer completed the final release validation on the target console.

## Save mutation and recovery

The platform layer reads the authoritative active Home Menu save path, performs interrupted Restore and Apply recovery before parsing, and exposes explicit manual backup, Restore and Apply operations. The save is modified only through the confirmed Restore or Apply actions. Both operations use staged replacement and recovery handling; Apply validates that the save still matches the source used to create the preview.

For exact paths and operation ordering, see [SAVE_LOCATION.md](SAVE_LOCATION.md), [WIIU_SAVE_PLATFORM.md](WIIU_SAVE_PLATFORM.md) and [APP_LIFECYCLE.md](APP_LIFECYCLE.md).

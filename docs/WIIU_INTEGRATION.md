# Wii U integration notes

## Metadata loading

The Wii U metadata provider is split into two parts:

- `WiiUMetadataProvider`: portable XML parsing, language mapping and in-memory catalog insertion.
- `WiiUInstalledMetadataProvider`: MCP enumeration and installed-title filesystem access, implemented under `src/platform/wiiu/`.

The adapter uses `MCP_TitleListByDeviceType(MCP_DEVICE_TYPE_AUTO, ...)`, not `MCP_TitleList`. The Homebrew on Menu plugin hooks `MCP_TitleList` and appends synthetic records with IDs in the `0005000F` family; the ordinary title count may not account for those appended records, so using that API with a count-sized buffer is unsafe. The integrated coordinator supplies the provider with the distinct non-folder title IDs referenced by the parsed save. The provider filters MCP's native title list against those IDs, so installed system applications such as Mii Maker are eligible for metadata lookup without parsing unrelated installed titles. Synthetic Homebrew metadata is resolved separately through `AddHomebrewTitleMetadata(titleId, error)`. Because Homebrew on Menu's ACP/FS hooks target the Wii U Menu process, this application must not rely on those hooks being active in its own process. The provider scans `SD:/wiiu/apps` and one subdirectory level, reproduces the plugin's `hash_string(relativePath)` algorithm (`hash = 37 * hash + byte`), and matches the resulting lower 32-bit ID. WUHB entries are read through libwuhbutils (`meta/meta.ini`, dedicated icon candidates, and GamePad/DRC artwork as a fallback); RPX entries use adjacent metadata/artwork when available. ACP and the virtual MLC icon path are fallback paths only. Only IDs actually present in the save are resolved.

Expected application sequence:

```cpp
titles_sorter::WiiUInstalledMetadataProvider metadataProvider;
std::string error;

if (!metadataProvider.Begin(error)) {
    // Report the MCP error and keep the application responsive.
}

// In the main loop, while metadataProvider.IsLoading():
metadataProvider.LoadStep(error); // Process no more than one title per frame.

if (metadataProvider.IsComplete()) {
    const auto& catalog = metadataProvider.Catalog();
    // Pass catalog to BuildDryRunPlan(...).
}
```

Call `LoadStep` once per frame, not in a tight loop. Missing or malformed title metadata is recorded as a warning and does not abort the remaining load. An MCP enumeration failure is fatal for the metadata-loading operation.

## Target build

Add these sources to the Wii U application's source list:

- `src/core/TitlesSorterCore.cpp`
- `src/core/WiiUMetadataProvider.cpp`
- `src/platform/wiiu/WiiUInstalledMetadataProvider.cpp`

Add `include/` to the include path. The target must use the WUT `coreinit/mcp.h` header. Do not add `tests/wut_mock` to a real Wii U build; it exists only for desktop compile-shape checking.

This repository includes the full application, Makefile, SDL UI and WUHB metadata. However, this source revision has not yet been cross-compiled with the maintainer's actual devkitPro/WUT installation or validated on physical hardware. There is deliberately no Home Menu save writer.

## Safety boundary

No function in this package writes, patches, serializes or replaces `BaristaAccountSaveFile.dat`. Save modification remains out of scope until the record mapping, page layout, folder rules, unknown bytes and integrity requirements are validated.


## MCP layout and synthetic Homebrew entries

The host mock mirrors the public WUT declaration of `MCPTitleListType`: `titleId` at `0x00`, `path[56]` at `0x0C`, `appType` at `0x44`, `indexedDevice[10]` at `0x56`, and total packed size `0x61`. Compile-time assertions guard these assumptions. See the upstream WUT header: <https://github.com/devkitPro/wut/blob/master/include/coreinit/mcp.h>.

The Homebrew on Menu plugin hooks `MCP_TitleList` and appends synthetic title records whose path is `/custom/...`; its source hashes the executable path relative to `/vol/external01/` using `hash = 37 * hash + byte`, and reads WUHB metadata from `meta/meta.ini`. Source: <https://github.com/wiiu-env/homebrew_on_menu_plugin/blob/main/src/main.cpp>. The application reproduces that ID mapping by scanning `SD:/wiiu/apps` and one subdirectory level, then reads WUHB metadata and icon candidates directly with libwuhbutils. It prefers `meta/iconTex.tga` and common icon aliases, then tries the bundle's TV/GamePad artwork. SDL_image is tried first; a bounded in-memory TGA decoder handles uncompressed and RLE true-colour/grayscale variants if SDL_image rejects a valid TGA. This avoids depending on plugin hooks that target the Wii U Menu process. The build requires libwuhbutils headers/library and the runtime WUHBUtilsModule; the latter must be present in the Aroma environment. RPX metadata/artwork is read from adjacent files if available, otherwise the executable filename and generic icon are used.

For native Wii U system titles, MCP provides the installed title path and the provider reads `meta/meta.xml` and `meta/iconTex.tga`, with ACP as a name/publisher fallback. Special Home Menu channels use documented nominal IDs: Disc is `00050010-10055000` (Product Code N/A), and the vWii launcher is `00050010-10004000` (OSv0/cafe2wii; no Product Code listed). The Disc channel name is localized using the Wii U system language (for example, `Disco` in Italian). Its icon lookup tries the nominal system-title path and optical-volume metadata paths. Wii tries OSv0/OSv1 paths before legacy compatibility candidates. These files/paths are system-dependent, so exact icon availability still requires console validation. If one media bank contains a `0x05` Disc or `0x09` vWii record and the other bank contains a conflicting record, the parser uses the explicitly typed special record for preview identity while preserving both original records unchanged. Both Disc and Wii participate in sorting like ordinary channels; when a selected field has no value (such as Product Code), the planner uses the Title ID as a stable sort fallback and the preview displays `N/A` for the missing Product Code. XML entities such as `&amp;` and `&apos;` are decoded in metadata values. Unknown records remain fixed.

## Integrated startup coordinator (v0.5)

`WiiUTitlesSorterCoordinator` combines the portable session with the Wii U metadata provider:

1. `Begin(saveBytes, error)` parses the caller-supplied account save bytes, collects distinct non-folder title IDs from the main menu and active folders, and starts MCP enumeration filtered to those IDs.
2. `Update(error)` processes at most one native `meta.xml` per frame.
3. Once native metadata is complete, it scans only title IDs actually present in the parsed main menu and active folder contents for the synthetic `0005000F` Homebrew on Menu family.
4. It resolves at most one such Homebrew title per `Update()` by matching the ID against the SD executable-path hash, then reads WUHB metadata/icon data through libwuhbutils (or adjacent RPX metadata/artwork).
5. It supplies the completed in-memory catalog to `TitlesSorterSession` and transitions to `Ready`. Preview labels and icons can then use metadata for system titles as well as games.

The coordinator does not discover or open the account save file. The exact save access mechanism and active-account path remain to be verified on hardware. ACP must already be initialized by the application before `Begin()`.

When `IsReady()` is true, use `Session()` to edit sort options and generate the in-memory preview. The coordinator and session remain read-only with respect to the save.

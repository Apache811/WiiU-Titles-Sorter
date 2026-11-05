# Memory and resource audit

This document records source-level memory/resource decisions and the optimisation work included in v1.0.0. It is not a benchmark report: no numerical frame-time or heap-peak improvement is claimed unless a measurement is explicitly recorded.

## Wii U target compiler and entry point

- `makeapp.mk` pins `CC`, `CXX`, and `LD` to `$(DEVKITPPC)/bin/powerpc-eabi-*`.
- The target build checks that the C++ compiler exists and `-dumpmachine` returns `powerpc-eabi`. The host UCRT64/MinGW compiler is confined to `maketests.mk`.
- The RPX link explicitly passes `$(RPXSPECS)`, the WUT-provided specs, which supply the RPX entry point (`__rpx_start`) and WUT linker script.

## Bounded allocations

- Home Menu save input is size-checked against `kExpectedHomeMenuSaveSize` before allocation/read.
- Metadata XML reads are bounded to 256 KiB per title; system language XML is bounded to 64 KiB.
- MCP title enumeration now rejects counts above 4,096 before allocating its title-list vector. At the asserted `MCPTitleListType` size of 0x61 bytes, the title-list buffer is bounded to under 400 KiB.
- Renderer text texture cache is bounded to 128 entries (reduced from 256). FIFO eviction now uses `std::deque::pop_front()` rather than erasing the first element of a vector.
- Embedded font data is copied once into renderer-owned storage so additional TTF sizes can reopen independent RW streams. The bundled font is about 760 KB; this is a deliberate bounded heap cost.

## Resource ownership reviewed

- Renderer `Shutdown()` destroys cached SDL textures, closes every cached `TTF_Font`, clears cache containers and font bytes; it is called by the destructor and explicit shutdown path.
- Text rendering frees each `SDL_Surface` after texture creation, including the texture-creation failure path.
- `TTF_OpenFontRW(..., freesrc=1, ...)` transfers RWops ownership to SDL_ttf.
- ACP aligned metadata storage is freed on both API failure and success paths.
- MCP handles in the title provider are closed on each checked failure path and after enumeration.
- MLC mount ownership is scoped by `MlcMount`; a successful mount is unmounted in the destructor. Mocha exposes no library-finalize function in the currently used API.

## Resource bounds and measurement scope

- STL containers and `std::string` still allocate dynamically. The Wii U build uses `-fno-exceptions`, so out-of-memory is not recoverable through C++ exceptions; allocation counts are bounded in the largest external-input paths, but not every standard-library allocation has a recoverable failure path.
- The 128-entry text texture cache and 48-entry title-icon cache bound entry counts, not exact GPU/texture bytes.
- Sanitizer execution was not part of the recorded optimisation measurements; no sanitizer result is claimed.
- The application was subsequently built and validated by the maintainer as part of the published v1.0.0 release.


## Build-time artwork preparation (2026-11-04)

- High-resolution source PNGs remain unchanged under `resources/`.
- `makeresources.mk` owns resource import and invokes `tools/prepare_assets.py` through `tools/resolve_python.sh`. The resolver selects Python 3.10+ with Pillow, including Windows Python installations discoverable from MSYS2.
- `makeapp.mk` no longer invokes Python or converts images. It validates the generated assets and embeds them from `build/assets/wiiu/`; the top-level `Makefile` always completes resource import before starting the Wii U application build.
- The renderer no longer decodes a 1300x1200 folder image and creates a temporary scaled SDL surface at startup. It decodes the already reduced PNG directly.
- With `unknown.png` and the transparent `lock.png` overlay included, the twelve embedded PNGs shrink from 14,289,190 bytes to 654,378 bytes (95.4%) on the current source artwork. The eight folder images are still uploaded as 256x235 textures and `unknown.png` as 256x256, so the final GPU footprint is determined by those runtime dimensions; the savings are in executable size, decode/scale work, and peak temporary CPU memory.
- The asset preparation script was exercised with Python 3 and Pillow and output dimensions were checked. The renderer also retains extracted WUHB icon bytes in shared memory for the duration of the metadata catalog; actual peak memory on console remains unmeasured. Wii U cross-compilation and hardware rendering still require the user's devkitPro environment and console.


## Pre-release source optimization pass — 2026-11-05

- Removed `TitlesSorterSession::BuildPreviewLabels()` and its fixed array of 300 `std::string` objects. Search confirmed the array was never read; visible preview labels are built by `GetPreviewPage()`. This removes redundant label generation and its string storage/allocations on save load, metadata updates and preview generation.
- `HomeMenuSavePatcher::ApplyAssignments` now uses fixed arrays bounded by `kMainSlotCount` for destination/source validation and empty-slot pairing. It rejects counts above that bound. This removes four short-lived heap containers per invocation.
- `BuildDryRunPlan` validates the fixed plan arrays in place instead of copying them into temporary vectors and allocating dynamic `vector<bool>` source sets.
- `HomeMenuSavePatcher` now scopes the source re-parse so its temporary `SaveModel` is destroyed before the patched candidate is reparsed.
- Apply releases the active-save comparison buffer after validation and the staged readback buffer after byte-for-byte verification. Backup creation releases source and verification buffers once the backup copy has been verified; Restore releases its staged readback buffer before commit. These changes lower peak live save-buffer memory without changing the validation sequence.
- Removed unused UI state left by the earlier confirmation flow: `confirmationPending`, `restoreSucceeded` and `applySucceeded`, plus unused request members in `UiFrame`. The preview-required result is routed through its dedicated state helper.
- Clang 17 strict diagnostics exposed an unnecessary lambda capture in `DrawTopBar`; removed it. No UI geometry or presentation was changed.

### Memory/performance characteristics

- Metadata icon payloads are retained in the catalog through `shared_ptr` and are capped per WUHB file at 2 MiB, but there is no aggregate byte budget across all Homebrew titles referenced by the save. This was not changed because an arbitrary global cap could silently remove artwork; measure peak memory on console before selecting a policy.
- The renderer bounds text textures to 128 entries and title icon textures to 48 entries. The cache limits bound entry counts, not exact GPU bytes.
- Sort-key generation still allocates per sortable item. The current maximum is bounded by Home Menu slot counts, and no representative Wii U timing profile is available; no speculative sort-key rewrite was made.
- The changes above are source-level allocation and lifetime reductions. They should not be interpreted as measured frame-time or peak-RAM percentages.


## Restore autobackup follow-up — 2026-11-05

- Restore now stages and verifies the selected backup before creating the optional pre-Restore autobackup. This preserves the selected content even if the retention pass removes its original file.
- After staging verification, both in-memory copies of the selected save are released before `CreateBackup` reads/verifies the active save. The staged file remains on disk. After autobackup completes, the staged file is read once more for the final post-install byte comparison.
- This reduces the overlapping full-save buffers during the autobackup phase from three to two. It is a source-level lifetime/peak-memory improvement; no on-console heap measurement is available.
- Regression tests cover a selected automatic backup being pruned by retention while Restore still succeeds, and a failed autobackup request aborting Restore without changing the active save or leaving a staging file.

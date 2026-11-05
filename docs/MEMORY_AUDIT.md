# Memory and resource audit

Scope: static review of the current source plus host tests. This is not a claim of a completed on-device heap profile or a passing sanitizer run.

## Wii U target compiler and entry point

- `makeapp.mk` pins `CC`, `CXX`, and `LD` to `$(DEVKITPPC)/bin/powerpc-eabi-*`.
- The target build checks that the C++ compiler exists and `-dumpmachine` returns `powerpc-eabi`. The host UCRT64/MinGW compiler is confined to `maketests.mk`.
- The RPX link explicitly passes `$(RPXSPECS)`, the WUT-provided specs. These include the RPX entry point (`__rpx_start`) and the WUT linker script. The previous build omitted these specs, which explains the `_start` warning; this must still be confirmed by the user's next target build.

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

## Remaining risks / limits

- STL containers and `std::string` still allocate dynamically. The Wii U build uses `-fno-exceptions`, so out-of-memory is not recoverable through C++ exceptions; allocation counts are bounded in the largest external-input paths, but not every standard-library allocation has a recoverable failure path.
- The 128 texture limit bounds entry count, not exact GPU/texture bytes. Runtime memory profiling on console is still required to quantify the peak.
- ASan/UBSan binaries compiled in the current container but could not run: ASan failed to reserve shadow memory due to container runtime constraints. No sanitizer pass is claimed.
- The earlier immediate error 160-2203 occurs before the first UI frame. The linker fix must be verified in the user's devkitPro build before attributing the crash to any runtime allocation.


## Build-time artwork preparation (2026-11-04)

- High-resolution source PNGs remain unchanged under `resources/`.
- `makeresources.mk` owns resource import and invokes `tools/prepare_assets.py` through `tools/resolve_python.sh`. The resolver selects Python 3.10+ with Pillow, including Windows Python installations discoverable from MSYS2.
- `makeapp.mk` no longer invokes Python or converts images. It validates the generated assets and embeds them from `build/assets/wiiu/`; the top-level `Makefile` always completes resource import before starting the Wii U application build.
- The renderer no longer decodes a 1300x1200 folder image and creates a temporary scaled SDL surface at startup. It decodes the already reduced PNG directly.
- With `unknown.png` and the transparent `lock.png` overlay included, the twelve embedded PNGs shrink from 14,289,190 bytes to 654,378 bytes (95.4%) on the current source artwork. The eight folder images are still uploaded as 256x235 textures and `unknown.png` as 256x256, so the final GPU footprint is determined by those runtime dimensions; the savings are in executable size, decode/scale work, and peak temporary CPU memory.
- The asset preparation script was exercised with Python 3 and Pillow and output dimensions were checked. The renderer also retains extracted WUHB icon bytes in shared memory for the duration of the metadata catalog; actual peak memory on console remains unmeasured. Wii U cross-compilation and hardware rendering still require the user's devkitPro environment and console.

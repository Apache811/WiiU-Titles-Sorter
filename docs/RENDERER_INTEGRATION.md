# SDL renderer integration

`TitlesSorterRenderer` is the SDL2/SDL_ttf presentation layer for the platform-independent
`TitlesSorterUiController` and `UiFrame` model.

## Initialization

The caller owns SDL and SDL_ttf initialization, window creation, and the SDL renderer.
Pass the embedded DejaVu Sans font bytes to `Initialize`; the renderer makes a private copy
and opens the required font sizes lazily. The renderer sets a 1920x1080 logical coordinate
space so TV and GamePad render targets can scale consistently.

```cpp
TitlesSorterRenderer renderer;
std::string error;
if (!renderer.Initialize(sdlRenderer, DejaVuSans_ttf,
                         DejaVuSans_ttf_size, error)) {
    // Log error and unwind SDL resources.
}

// Once per frame, after controller.Update() and controller.BuildFrame():
renderer.Draw(controller.BuildFrame(), elapsedMilliseconds);

renderer.Shutdown(); // before SDL_DestroyRenderer / TTF_Quit / SDL_Quit
```

The renderer does not own `SDL_Renderer`, initialize SDL, poll VPAD, or open the software
keyboard. Those remain responsibilities of the Wii U application shell. Folder artwork is
pre-scaled to 256x235 by the build-time asset pipeline; the renderer only decodes the
small generated PNGs and creates the corresponding SDL textures.

## Screen composition

- 1920x1080 logical resolution.
- Titles Info pale blue-grey background, blue gradient header/footer, white rounded cards.
- Header contains only `Titles Sorter`; no ALL/NAND/USB filters.
- SETTINGS shows the four approved sorting rows.
- GENERATING PREVIEW shows a time-driven rotating indicator.
- PREVIEW shows a 5x3 illustrative tile grid and page counter.
- Empty slots and folders have dedicated artwork; title, Disc, vWii and unknown slots use their resolved installed icon when available, otherwise the bundled `unknown.png` fallback.
- Pinned/fixed entries use the transparent `resources/lock.png` overlay, rendered at 60 × 60 logical pixels at the top-right of the entire card (icon plus title), not at the top-right of the artwork. The lock is an icon, not a text badge. Homebrew WUHB icons are decoded from in-memory image bytes returned by libwuhbutils; installed title icons retry both devoptab and CafeOS path forms. Disc tries optical-volume artwork and Wii tries regional Wii Menu icon paths. All hardware-specific path fallbacks require final visual validation on console.
- Generic tiles do not overlay `T`, `D` or `W` letters on top of the fallback artwork.

## Main loop responsibilities

1. Call `WiiUInputAdapter::Poll()` once per frame.
2. Pass each returned event to `TitlesSorterUiController::HandleInput`.
3. Call `TitlesSorterUiController::Update()` exactly once per frame.
4. Build a `UiFrame` and call `TitlesSorterRenderer::Draw`.
5. If `excludedPagesInputRequested` is true, use the Wii U software keyboard integration
   (`SearchKeyboard`) and pass the confirmed/cancelled result back to the UI controller.

Do not pass held-button state as repeated edges for Confirm, ZR, Back, or HOME.

## Validation boundary

The renderer's host tests compile the real renderer source against a deterministic SDL mock.
They test screen dispatch, drawing calls, cache/resource cleanup, and logical-size setup.
They do not validate actual SDL rendering, SDL_ttf glyph output, GPU behavior, Wii U GamePad
rendering, or the target devkitPro linker. A real WUT/SDL build remains required.

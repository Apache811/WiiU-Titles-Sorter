# Titles Sorter UI contract — v0.6

This release implements the platform-independent UI controller and frame model. It does **not** yet contain SDL drawing code or a Wii U executable. The renderer must consume `UiFrame`; VPAD polling must translate controller buttons to `UiInput`.

## Visual language

Preserve the Titles Info composition:
- 1920×1080 logical coordinate space (renderer may scale to TV output).
- Pale blue-grey background: RGB(239, 244, 249).
- Blue gradient top/bottom bars: RGB(27, 139, 224) to RGB(0, 91, 174).
- White rounded cards, DejaVu Sans.
- Top bar contains the application title only; no ALL/NAND/USB filters.
- Bottom bar shows only actions available on the current screen.

## Settings rows

1. Sort By — Title ID, Product Code, Long Name, Short Name, Publisher, Folder Name.
2. Folder Placement — Folders First / Mixed Alphabetically.
3. Sort Folder Contents — ON/OFF; immediate contents only.
4. Excluded Pages — expression such as `1-3,6,8`; empty or `None` means no exclusions.

D-pad Up/Down changes focus. Left/Right changes enum/boolean settings. A toggles boolean rows or requests text entry for Excluded Pages. A platform text-entry widget calls `SetExcludedPagesExpression`; invalid input is rejected without changing the active options. ZR begins preview generation. HOME requests exit.

## Generating Preview

The renderer displays “Generating Preview”. The main loop calls `TitlesSorterUiController::Update()` exactly once per frame. The session advances from GeneratingPreview to Preview or Error.

## Preview

Render `UiFrame::preview` as the experimental 5×3 page grid, using neutral placeholder artwork until icon paths have been verified. L/R changes page; B returns to Settings; HOME requests exit.

Preview is strictly derived from the dry-run plan. No save-writing operation is exposed by the UI controller.

## Platform boundary

`TitlesSorterUiController` contains no SDL/WUT dependencies. The Wii U frontend is responsible for:
- VPAD input polling and mapping to `UiInput`.
- SDL2/SDL_ttf drawing, text texture caching and scaling.
- Opening the system/software keyboard or a custom on-screen keyboard for excluded-page input.
- Lazy icon loading only after title icon path resolution is verified.

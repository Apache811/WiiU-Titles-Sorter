# Titles Sorter UI contract — v1.0.0

This document describes the current controller, frame model and SDL renderer contract. The Wii U frontend maps VPAD input to `UiInput`; the controller produces `UiFrame`; the renderer presents that frame.

## Shared visual language

- 1920×1080 logical coordinate space, scaled by SDL.
- Pale blue-grey background (RGB 239, 244, 249), blue gradient top/bottom bars, white rounded cards and embedded DejaVu Sans.
- Top-bar order: OPTIONS → PREVIEW → APPLY. The ZL/ZR navigation cycle is explicit and fixed:

  | Current section | ZL | ZR |
  |---|---|---|
  | OPTIONS | APPLY | PREVIEW |
  | PREVIEW | OPTIONS | APPLY |
  | APPLY | PREVIEW | OPTIONS |

- HOME remains owned by WHB ProcUI and exits through the application lifecycle.
- Decision screens show HOME/Exit and A/Confirm plus B/Cancel. Operation-result screens show HOME/Exit and A/Confirm; B is ignored.

## OPTIONS

- Sort By: Title ID, Product Code, Long Name, Short Name.
- Folder Placement: Folders First / Mixed Alphabetically.
- Sort Folder Contents: ON/OFF.
- Excluded Pages: parsed expression edited through the Wii U software keyboard.
- Preserve the established OPTIONS layout, interaction and `Edit Text` bottom-bar action.

## PREVIEW

- Entering PREVIEW from OPTIONS or APPLY starts generation immediately; A is not required.
- Render the 5×3 grid from `UiFrame::preview`.
- L/R changes pages with wraparound. A enters a folder; B returns to the previous Home Menu page/selection. ZL/ZR navigate to the adjacent sections as listed above.
- The generating screen is shown before the generated preview is presented.

## APPLY

Five full-width controls in this order: Autobackup, Max Number of Autobackups to Keep, Create Backup, Restore Backup, Apply the Previewed Layout. The established control geometry and main APPLY footer are fixed for the v1.0 UI.

- Autobackup is toggled with D-pad Left/Right. The maximum automatic-backup count is edited with the Wii U software keyboard and accepts 1–100.
- Create Backup executes immediately on A; it does not ask for a second confirmation. Its result screen reports success/failure and the path or error.
- Restore opens the backup selector. Manual and automatic backups are interleaved newest-first by timestamp. D-pad Up/Down selects; A opens the decision screen; A confirms; B returns to the selector. When Autobackup is enabled, the current active save is backed up before replacement. The result screen reports the safety-backup path when one was created, including when a later Restore step fails.
- Apply requires a current preview and opens an A/B decision screen. If no current preview exists, `Generate a Preview before applying a layout.` is shown in the same operation-result presentation used by Create Backup.
- Backup, Restore and Apply result screens use consistent typography, positioning and footer behaviour. Result screens accept A only.
- After either a successful or failed Restore/Apply, the result remains visible until A is pressed. Only then does the application reload the active Home Menu save and installed-title metadata. B and other inputs do not trigger the reload.
- Apply reports `Layout Applied !!!` or `Apply Failed !!!`. If an autobackup was created, its path is included in the detail text, including when a later Apply stage fails.

## Loading and error messages

- The loading screen describes its work as reading the Home Menu save and installed-title metadata; it is not a save-progress indicator and does not persist settings.
- Preview generation has its own `Generating Preview` screen.
- Startup/load errors remain on the error screen until the user returns or exits through HOME/ProcUI.

## Platform boundary

`TitlesSorterUiController` remains independent of SDL/WUT. The Wii U frontend owns VPAD polling, software-keyboard integration, save-platform operations and lifecycle transitions. `TitlesSorterRenderer` owns SDL textures/fonts and bounded text/icon caches.

# Wii U application lifecycle

The target entry point is `src/platform/wiiu/main.cpp`.

## Startup and main-loop ownership

1. Initialize WHB ProcUI, UDP logging and VPAD.
2. Initialize Mocha and attempt the optional MLC mount; initialize ACP, SDL2, SDL_ttf, the SDL window/renderer and the embedded font renderer.
3. If a required platform or renderer component cannot initialize, log the critical error, release already-initialized services and return `-1`.
4. Once the required components are ready, enter exactly one application-lifetime `while (WHBProcIsRunning())` loop.
5. Save acquisition, incremental metadata loading, startup-error presentation and interactive UI are phases handled by that loop. Apply/Restore result acknowledgement (A), after either success or failure, returns to the same loop and re-enters save acquisition; no nested application-lifecycle loop is created.
6. Save acquisition or metadata-loading failures are non-critical to process lifecycle: the loop renders an error screen and continues polling input until HOME requests exit or ProcUI requests shutdown.
7. After `coordinator.Begin()` has created the session, load persisted settings into that session. Do not load them before `Begin()`: it resets the coordinator session.
8. The coordinator processes at most one native title metadata item or one Homebrew metadata item per loading frame.
9. Before parsing the active save, the platform adapter attempts interrupted Restore recovery followed by interrupted Apply recovery.
10. On shutdown, finalize ACP, release Mocha/storage, shut down SDL resources, VPAD, ProcUI and UDP logging.

The single loop keeps lifecycle ownership explicit. The v1.0.0 application lifecycle and HOME/foreground behaviour were included in the maintainer's final Wii U validation.

## UI navigation and actions

- The top-bar order is OPTIONS → PREVIEW → APPLY.
- From OPTIONS, ZL opens APPLY and ZR generates PREVIEW immediately; no A confirmation is required to enter Preview.
- From PREVIEW, ZL returns to OPTIONS and ZR opens APPLY.
- From APPLY, ZL generates PREVIEW immediately and ZR returns to OPTIONS.
- OPTIONS retains the v0.5 sorting controls, Excluded Pages keyboard field and bottom-bar interaction.
- PREVIEW retains its v0.5 grid, L/R page navigation and A/B folder navigation.
- APPLY exposes Autobackup, maximum automatic backup count, Create Backup, Restore Backup and Apply the Previewed Layout.
- Create Backup runs immediately on A and then displays a result screen. Apply requires an A/B decision. Restore requires selecting a backup and an A/B decision; B cancels. Operation-result screens accept A only.
- The Wii U software keyboard edits the maximum automatic backup count. Invalid values leave the previous valid value unchanged.
- Settings are persisted separately from the Home Menu save. The maximum automatic backup count is currently constrained to 1–100.

## Save mutation and recovery

- Manual backup copies the active save byte-for-byte into `fs:/vol/external01/wiiu/apps/TitlesSorter/backups`.
- Apply validates the active save against the source used by the preview, builds and validates the candidate, optionally creates an autobackup, stages the candidate beside the active save, and uses a recovery rename sequence before commit.
- Restore validates and stages the selected backup before retention can run. When Autobackup is enabled, it then creates a byte-for-byte automatic backup of the currently active save; if that backup cannot be completed, Restore is aborted and the staged candidate is removed. Restore then uses a recovery rename sequence.
- After the user acknowledges an Apply or Restore result with A, the application re-enters save acquisition and reloads the session and metadata, regardless of operation success or failure. B and other inputs do not acknowledge result screens. Interrupted transaction recovery runs before the next parse.
- Backup, Restore and Apply are integrated with the UI and Wii U platform adapter. Restore and Apply use staged replacement and recovery files; maintain independent backups of valuable data as a prudent data-safety practice.

## Release validation

The maintainer completed the final validation of the Wii U application and published v1.0.0. The development-stage test history and environment-specific records are retained in `TEST_RESULTS.md`; they are historical evidence from the individual increments, not an outstanding release checklist.

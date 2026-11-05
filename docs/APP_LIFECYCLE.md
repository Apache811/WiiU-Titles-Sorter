# Wii U application lifecycle

The target entry point is `src/platform/wiiu/main.cpp`.

## Startup and main-loop ownership

1. Initialize WHB ProcUI, UDP logging and VPAD.
2. Initialize Mocha and attempt the optional MLC mount; initialize ACP, SDL2, SDL_ttf, the SDL window/renderer and the embedded font renderer.
3. If a required platform or renderer component cannot initialize, log the critical error, release already-initialized services and return `-1`.
4. Once the required components are ready, enter exactly one application-lifetime `while (WHBProcIsRunning())` loop.
5. Save acquisition, incremental metadata loading, startup-error presentation and interactive UI are phases handled by that loop. There are no nested application-lifecycle loops.
6. Save acquisition or metadata-loading failures are non-critical to the process lifecycle: the loop renders an error screen and continues polling input until HOME requests exit or ProcUI requests shutdown.
7. After `coordinator.Begin()` has created the session, load persisted settings into that session. Do not load them before `Begin()`: it resets the coordinator session.
8. The coordinator processes at most one native title metadata item or one Homebrew metadata item per loading frame.
9. On shutdown, finalize ACP, release Mocha/storage, shut down SDL resources, VPAD, ProcUI and UDP logging.

The single loop makes lifecycle ownership explicit, but does not by itself prove or guarantee that the HOME → Continue black-screen issue is fixed. The SDL renderer's foreground/background resource behaviour still requires testing on the console.

## Safety properties

- The application only opens the Home Menu save for reading. No write, rename, truncate, serializer, backup or restore path is called.
- A redirected Homebrew-on-Menu save remains authoritative according to the save reader. An invalid redirected save is not replaced with a fallback MLC copy.
- The preview is generated in memory. Its slot/page mapping is still explicitly experimental and must not be used to write the save.
- A successfully generated preview is retained while the complete set of sort options and the metadata-catalog reference remain unchanged. Returning to Settings does not discard it; the next ZR returns directly to the cached preview. Changing an option invalidates the cached result, so the next ZR generates a fresh plan.

## Validation status

- The revised Wii U source has not been compiled in this environment because the devkitPPC cross-compiler is unavailable.
- Host tests could not run here because the required MSYS2 UCRT64 compiler (`/ucrt64/bin/g++.exe`) is unavailable.
- The HOME → Continue lifecycle must be tested on the actual console after rebuilding with the user's devkitPro/WUT toolchain.

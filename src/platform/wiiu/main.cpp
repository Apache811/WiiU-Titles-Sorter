#include "platform/wiiu/WiiUHomeMenuSavePlatform.hpp"
#include "core/TitlesSorterSettingsStore.hpp"
#include "platform/wiiu/WiiUInputAdapter.hpp"
#include "platform/wiiu/WiiUTitlesSorterCoordinator.hpp"
#include "ui/TitlesSorterRenderer.hpp"
#include "ui/TitlesSorterUI.hpp"
#include "platform/wiiu/SearchKeyboard.hpp"

#include <whb/proc.h>
#include <whb/log.h>
#include <whb/log_udp.h>
#include <vpad/input.h>
#include <nn/acp.h>
#include <mocha/mocha.h>
#include <SDL.h>
#include <SDL_ttf.h>
#include <SDL_image.h>

#include "DejaVuSans_ttf.h"
#include "Log.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

namespace {

constexpr const char* kApplicationTitle = "Titles Sorter";
constexpr const char* kSettingsPath = "fs:/vol/external01/wiiu/apps/TitlesSorter/settings.ini";
bool g_mochaInitialized = false;
bool g_usbMounted = false;
bool g_mlcMounted = false;

void Log(const char* message) {
    TitleSorterLog("Main", message);
}

void LogStorage(const char* message) {
    TitleSorterLog("Storage", message);
}

bool InitializeVideo(SDL_Window*& window, SDL_Renderer*& renderer, std::string& error) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        error = std::string("SDL_Init failed: ") + SDL_GetError();
        return false;
    }
    if (TTF_Init() != 0) {
        error = std::string("TTF_Init failed: ") + TTF_GetError();
        SDL_Quit();
        return false;
    }
    // Match Titles Info image subsystem initialization. TGA is supported by
    // SDL_image's IMG_Load path without requiring an additional IMG_INIT flag.
    IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);

    window = SDL_CreateWindow(kApplicationTitle, 0, 0,
        titles_sorter::TitlesSorterRenderer::LogicalWidth,
        titles_sorter::TitlesSorterRenderer::LogicalHeight, 0);
    if (window == nullptr) {
        error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (renderer == nullptr) {
        error = std::string("SDL_CreateRenderer failed: ") + SDL_GetError();
        SDL_DestroyWindow(window);
        window = nullptr;
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return false;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    return true;
}

void ShutdownVideo(SDL_Window* window, SDL_Renderer* renderer,
                   titles_sorter::TitlesSorterRenderer& uiRenderer) {
    uiRenderer.Shutdown();
    if (renderer != nullptr) SDL_DestroyRenderer(renderer);
    if (window != nullptr) SDL_DestroyWindow(window);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

bool InitializeStorage() {
    LogStorage("Initializing Mocha.");
    if (Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) {
        LogStorage("Mocha_InitLibrary failed.");
        return false;
    }
    g_mochaInitialized = true;
    LogStorage("Mocha initialized.");

    // Match Titles Info: expose the title volumes using the exact device names
    // expected by MCP metadata paths (storage_usb01:/... and storage_mlc01:/...).
    // Both mounts are optional so a missing USB device does not prevent NAND use.
    LogStorage("Mounting USB title volume.");
    if (Mocha_MountFS("storage_usb01", nullptr, "/vol/storage_usb01") == MOCHA_RESULT_SUCCESS) {
        g_usbMounted = true;
        LogStorage("USB title volume mounted.");
    } else {
        LogStorage("USB title volume mount failed; USB title icons may be unavailable.");
    }

    LogStorage("Mounting NAND title volume.");
    if (Mocha_MountFS("storage_mlc01", nullptr, "/vol/storage_mlc01") == MOCHA_RESULT_SUCCESS) {
        g_mlcMounted = true;
        LogStorage("NAND title volume mounted.");
    } else {
        LogStorage("NAND title volume mount failed; NAND title icons may be unavailable.");
    }
    return true;
}

void CleanupStorage() {
    if (g_usbMounted) {
        LogStorage("Unmounting USB title volume.");
        Mocha_UnmountFS("storage_usb01");
        g_usbMounted = false;
    }
    if (g_mlcMounted) {
        LogStorage("Unmounting NAND title volume.");
        Mocha_UnmountFS("storage_mlc01");
        g_mlcMounted = false;
    }
    if (g_mochaInitialized) {
        LogStorage("Deinitializing Mocha.");
        Mocha_DeInitLibrary();
        g_mochaInitialized = false;
    }
}

void DrawError(titles_sorter::TitlesSorterRenderer& renderer, const std::string& message) {
    titles_sorter::UiFrame frame{};
    frame.screen = titles_sorter::Screen::Error;
    frame.message = message;
    frame.footer = "HOME: System Menu";
    renderer.Draw(frame, SDL_GetTicks());
}

enum class StartupPhase : std::uint8_t {
    ReadSave,
    LoadMetadata,
    Ready,
    Error
};

} // namespace

int main(int, char**) {
    WHBProcInit();
    WHBLogUdpInit();
    Log("Application process initialized.");

    VPADInit();
    Log("VPAD initialized.");

    // These services are required to access the active Home Menu save.
    // Failure here is critical because the application cannot proceed safely.
    if (!InitializeStorage()) {
        CleanupStorage();
        VPADShutdown();
        WHBProcShutdown();
        WHBLogUdpDeinit();
        return -1;
    }

    ACPInitialize();
    Log("ACP initialized.");

    SDL_Window* window = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    std::string error;
    if (!InitializeVideo(window, sdlRenderer, error)) {
        TitleSorterLogf("Main", "Critical initialization failure: %s", error.c_str());
        ACPFinalize();
        CleanupStorage();
        VPADShutdown();
        WHBProcShutdown();
        WHBLogUdpDeinit();
        return -1;
    }

    titles_sorter::TitlesSorterRenderer renderer;
    if (!renderer.Initialize(sdlRenderer, DejaVuSans_ttf,
                             static_cast<std::size_t>(DejaVuSans_ttf_size), error)) {
        TitleSorterLogf("Main", "Critical renderer initialization failure: %s", error.c_str());
        ShutdownVideo(window, sdlRenderer, renderer);
        ACPFinalize();
        CleanupStorage();
        VPADShutdown();
        WHBProcShutdown();
        WHBLogUdpDeinit();
        return -1;
    }

    titles_sorter::WiiUTitlesSorterCoordinator coordinator;
    titles_sorter::TitlesSorterSettingsStore settingsStore;
    titles_sorter::TitlesSorterUiController ui(coordinator.Session());
    titles_sorter::WiiUInputAdapter input;
    SearchKeyboard searchKeyboard;
    if (!searchKeyboard.Initialize()) Log("Native Wii U software keyboard initialization failed.");
    else Log("Native Wii U software keyboard initialized.");
    titles_sorter::WiiUHomeMenuSavePlatform savePlatform;
    titles_sorter::WiiUHomeMenuSaveSnapshot snapshot;

    titles_sorter::UiFrame loadingFrame{};
    loadingFrame.screen = titles_sorter::Screen::LoadingData;
    loadingFrame.footer = "Reading active Home Menu data...";

    StartupPhase phase = StartupPhase::ReadSave;
    Log("Initialization complete. Entering the application lifecycle loop.");

    // One owner for the entire application lifetime. Startup, loading, errors,
    // and interactive UI are phases within this loop, not separate lifecycle loops.
    while (WHBProcIsRunning()) {
        if (phase == StartupPhase::ReadSave) {
            renderer.Draw(loadingFrame, SDL_GetTicks());
            Log("Reading the active account save (read-only).");
            if (!savePlatform.ReadActiveSave(snapshot, error)) {
                TitleSorterLogf("Main", "Save initialization failed: %s", error.c_str());
                phase = StartupPhase::Error;
            } else if (!coordinator.Begin(snapshot.bytes, error)) {
                TitleSorterLogf("Main", "Catalog initialization failed: %s", error.c_str());
                phase = StartupPhase::Error;
            } else {
                // Begin() resets and recreates the session. Load preferences only
                // after it succeeds, otherwise the coordinator would erase them.
                std::string settingsError;
                if (!settingsStore.Load(kSettingsPath,
                                        coordinator.Session().Options(), settingsError)) {
                    TitleSorterLogf("Settings",
                        "Unable to load settings; defaults will be used: %s",
                        settingsError.c_str());
                } else {
                    Log("Settings loaded (or defaults selected because no settings file exists).");
                }

                if (coordinator.IsReady()) {
                    Log("Startup complete. Catalog is ready.");
                    phase = StartupPhase::Ready;
                } else {
                    Log("Save acquired. Starting incremental metadata enumeration.");
                    phase = StartupPhase::LoadMetadata;
                }
            }
            SDL_Delay(1);
            continue;
        }

        if (phase == StartupPhase::LoadMetadata) {
            renderer.Draw(loadingFrame, SDL_GetTicks());
            (void)input.Poll(); // Ignore navigation until the catalog is complete.
            coordinator.Update(error); // At most one metadata item per frame.
            if (coordinator.IsReady()) {
                Log("Startup complete. Catalog is ready.");
                phase = StartupPhase::Ready;
                (void)input.Poll(); // Discard a trigger from the loading-to-UI transition.
            } else if (coordinator.State() == titles_sorter::CoordinatorState::Failed) {
                if (error.empty()) error = "Metadata loading failed.";
                TitleSorterLogf("Main", "Metadata loading failed: %s", error.c_str());
                phase = StartupPhase::Error;
            }
            SDL_Delay(1);
            continue;
        }

        if (phase == StartupPhase::Error) {
            DrawError(renderer, error);
            // HOME is intentionally not consumed as a VPAD UI command.
            // WHBProcIsRunning() owns the HOME-menu foreground/background and
            // exit decision; Continue must return to this same lifecycle loop.
            (void)input.Poll();
            SDL_Delay(16);
            continue;
        }

        // Ready: normal UI. WHBProcIsRunning remains the single lifecycle gate.
        if (searchKeyboard.IsActive()) {
            searchKeyboard.Update();
            bool confirmed = false;
            std::string keyboardResult;
            if (searchKeyboard.ConsumeResult(confirmed, keyboardResult)) {
                if (confirmed) {
                    std::string validationError;
                    if (!ui.SetExcludedPagesExpression(keyboardResult, validationError))
                        TitleSorterLogf("UI", "Excluded Pages rejected: %s", validationError.c_str());
                    else {
                        Log("Excluded Pages updated from the Wii U software keyboard.");
                        if (!settingsStore.SaveIfChanged(kSettingsPath, coordinator.Session().Options(), validationError))
                            TitleSorterLogf("Settings", "Unable to persist settings: %s", validationError.c_str());
                    }
                } else {
                    ui.CancelExcludedPagesInputRequest();
                    Log("Excluded Pages input cancelled.");
                }
            }
            renderer.Draw(ui.BuildFrame(), SDL_GetTicks());
            SDL_Delay(1);
            continue;
        }

        const titles_sorter::UiInputBatch batch = input.Poll();
        const bool wasOnSettingsScreen =
            coordinator.Session().CurrentScreen() == titles_sorter::Screen::Settings;
        bool optionsMayHaveChanged = false;
        for (std::size_t i = 0; i < batch.count; ++i) {
            const titles_sorter::UiInput event = batch.events[i];
            ui.HandleInput(event);
            optionsMayHaveChanged = optionsMayHaveChanged ||
                (wasOnSettingsScreen &&
                 (event == titles_sorter::UiInput::Left ||
                  event == titles_sorter::UiInput::Right ||
                  event == titles_sorter::UiInput::Confirm));
        }
        if (optionsMayHaveChanged &&
            !settingsStore.SaveIfChanged(kSettingsPath, coordinator.Session().Options(), error))
            TitleSorterLogf("Settings", "Unable to persist settings: %s", error.c_str());
        ui.Update();
        if (ui.ConsumeExcludedPagesInputRequest()) {
            const std::string currentValue = ui.BuildFrame().settings[3].value;
            const std::string initialText = currentValue == "None" ? std::string{} : currentValue;
            if (!searchKeyboard.Request(initialText, "Example: 1-3, 8, 11")) {
                Log("Unable to open the Wii U software keyboard for Excluded Pages.");
                ui.CancelExcludedPagesInputRequest();
            } else {
                Log("Wii U software keyboard requested for Excluded Pages.");
            }
        }
        renderer.Draw(ui.BuildFrame(), SDL_GetTicks());
        SDL_Delay(1);
    }

    Log("Main loop exited; shutting down application services.");
    searchKeyboard.Shutdown();
    ACPFinalize();
    CleanupStorage();
    ShutdownVideo(window, sdlRenderer, renderer);
    VPADShutdown();
    WHBProcShutdown();
    WHBLogUdpDeinit();
    return 0;
}

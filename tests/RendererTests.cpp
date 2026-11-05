#include "ui/TitlesSorterRenderer.hpp"
#include "SdlMock.hpp"

#include <array>
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
#include <string>

using namespace titles_sorter;

int main() {
    ResetSdlMockStats();
    SDL_Renderer renderer{};
    std::array<std::uint8_t, 16> fakeFont{};
    TitlesSorterRenderer ui;
    std::string error;
    assert(ui.Initialize(&renderer, fakeFont.data(), fakeFont.size(), error));
    assert(error.empty());
    assert(GetSdlMockStats().logicalWidth == TitlesSorterRenderer::LogicalWidth);
    assert(GetSdlMockStats().logicalHeight == TitlesSorterRenderer::LogicalHeight);

    UiFrame frame;
    frame.screen = Screen::Settings;
    frame.footer = "D-PAD: Navigate   LEFT/RIGHT: Change   ZR: Preview   HOME: Exit";
    frame.settings[0] = {"Sort By", "Long Name", true, true};
    frame.settings[1] = {"Folder Placement", "Folders First", false, true};
    frame.settings[2] = {"Sort Folder Contents", "OFF", false, true};
    frame.settings[3] = {"Excluded Pages", "1-3,6,8", false, true};
    ui.Draw(frame, 0);
    assert(GetSdlMockStats().clearCalls == 1U);
    assert(GetSdlMockStats().presentCalls == 1U);
    assert(GetSdlMockStats().copyCalls > 0U);

    frame.screen = Screen::LoadingData;
    ui.Draw(frame, 625);
    assert(GetSdlMockStats().presentCalls == 2U);

    frame.screen = Screen::GeneratingPreview;
    ui.Draw(frame, 1250);
    assert(GetSdlMockStats().presentCalls == 3U);

    frame.screen = Screen::Preview;
    frame.preview.pageNumber = 2;
    frame.previewPageCount = 20;
    frame.preview.slots[0].occupied = true;
    frame.preview.slots[0].kind = ItemKind::Folder;
    frame.preview.slots[0].label = "Homebrews";
    frame.preview.slots[0].pinned = true;
    frame.preview.slots[1].occupied = true;
    frame.preview.slots[1].kind = ItemKind::Title;
    frame.preview.slots[1].titleId = 0x0005000012345678ULL;
    frame.preview.slots[1].label = "Example Game";
    // Minimal 1x1, 32-bit uncompressed TGA. IMG_Load_RW is deliberately
    // unavailable in the host mock, so this exercises the TGA fallback decoder.
    auto tga = std::make_shared<std::vector<std::uint8_t>>(22U, 0U);
    (*tga)[2] = 2U;
    (*tga)[12] = 1U;
    (*tga)[14] = 1U;
    (*tga)[16] = 32U;
    (*tga)[17] = 0x20U;
    (*tga)[18] = 0x30U; (*tga)[19] = 0x20U; (*tga)[20] = 0x10U; (*tga)[21] = 0xFFU;
    frame.preview.slots[1].iconData = tga;
    const std::size_t texturesBeforePreview = GetSdlMockStats().createdTextures;
    ui.Draw(frame, 2250);
    assert(GetSdlMockStats().presentCalls == 4U);
    assert(GetSdlMockStats().createdTextures > texturesBeforePreview);

    ui.Shutdown();
    assert(GetSdlMockStats().destroyedTextures == GetSdlMockStats().createdTextures);
    std::cout << "Renderer tests passed.\n";
    return 0;
}

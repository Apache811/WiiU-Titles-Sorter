#include "ui/TitlesSorterUI.hpp"

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {
void WriteBe64(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint64_t value) {
    for (unsigned i = 0; i < 8U; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> ((7U - i) * 8U));
}
void WriteBe32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4U; ++i)
        bytes[offset + i] = static_cast<std::uint8_t>(value >> ((3U - i) * 8U));
}
}

int main() {
    using namespace titles_sorter;
    std::vector<std::uint8_t> bytes(kExpectedSaveSize, 0U);
    constexpr std::uint64_t titleId = 0x0005000012345600ULL;
    WriteBe64(bytes, kMainNandOffset, titleId);
    WriteBe32(bytes, kMainNandOffset + 8U, 0x01U);

    TitlesSorterSession session;
    std::string error;
    assert(session.LoadSave(bytes, error));
    TitlesSorterUiController ui(session);

    UiFrame frame = ui.BuildFrame();
    assert(frame.screen == Screen::Settings);
    assert(frame.settings[0].selected);
    assert(frame.settings[0].label == "Sort By");
    assert(frame.settings[0].value == "Long Name");

    ui.HandleInput(UiInput::Right);
    assert(session.Options().field == SortField::ShortName);
    ui.HandleInput(UiInput::Left);
    assert(session.Options().field == SortField::LongName);

    ui.HandleInput(UiInput::Down);
    assert(ui.FocusedSettingsRow() == SettingsRow::FolderPlacement);
    ui.HandleInput(UiInput::Right);
    assert(session.Options().folderPlacement == FolderPlacement::MixedAlphabetically);

    ui.HandleInput(UiInput::Down);
    ui.HandleInput(UiInput::Confirm);
    assert(!session.Options().sortFolderContents);

    ui.HandleInput(UiInput::Down);
    ui.HandleInput(UiInput::Confirm);
    assert(ui.BuildFrame().excludedPagesInputRequested);
    assert(ui.ConsumeExcludedPagesInputRequest());
    assert(!ui.BuildFrame().excludedPagesInputRequested);
    assert(ui.SetExcludedPagesExpression("1-3, 6, 8", error));
    assert((session.Options().excludedPages == std::vector<std::uint32_t>{1U,2U,3U,6U,8U}));
    assert(!ui.BuildFrame().excludedPagesInputRequested);
    assert(!ui.SetExcludedPagesExpression("0, 4", error));
    assert(!error.empty());
    assert((session.Options().excludedPages == std::vector<std::uint32_t>{1U,2U,3U,6U,8U}));
    assert(ui.SetExcludedPagesExpression("None", error));
    assert(session.Options().excludedPages.empty());

    // Either shoulder button selects the other top-bar screen.
    ui.HandleInput(UiInput::Zl);
    assert(session.CurrentScreen() == Screen::GeneratingPreview);
    ui.Update();
    assert(session.CurrentScreen() == Screen::Preview);
    ui.HandleInput(UiInput::Zr);
    assert(session.CurrentScreen() == Screen::Settings);
    ui.HandleInput(UiInput::Zr);
    assert(session.CurrentScreen() == Screen::Preview ||
           session.CurrentScreen() == Screen::GeneratingPreview);
    ui.Update();
    assert(session.CurrentScreen() == Screen::Preview);
    frame = ui.BuildFrame();
    assert(frame.previewPageCount == 20U);
    assert(frame.preview.slots[0].occupied);
    assert(frame.preview.slots[0].titleId == titleId);

    ui.HandleInput(UiInput::R);
    assert(session.PreviewPageNumber() == 2U);
    ui.HandleInput(UiInput::L);
    assert(session.PreviewPageNumber() == 1U);
    ui.HandleInput(UiInput::L); // Wrap first Home Menu page to the last.
    assert(session.PreviewPageNumber() == 20U);
    ui.HandleInput(UiInput::R);
    assert(session.PreviewPageNumber() == 1U);
    ui.HandleInput(UiInput::Zl);
    assert(session.CurrentScreen() == Screen::Settings);

    // A folder in the Home Menu opens a separate preview of its planned contents.
    std::vector<std::uint8_t> folderSave(kExpectedSaveSize, 0U);
    constexpr std::uint64_t folderRecordId = 1ULL;
    constexpr std::uint64_t childTitleId = 0x0005000012345600ULL;
    WriteBe64(folderSave, kMainNandOffset, folderRecordId);
    WriteBe32(folderSave, kMainNandOffset + 8U, 0x10U);
    const std::size_t folderBlock = kFolderRegionOffset;
    WriteBe64(folderSave, folderBlock, childTitleId);
    WriteBe32(folderSave, folderBlock + 8U, 0x01U);
    const char folderName[] = "Folder";
    for (std::size_t i = 0; i < sizeof(folderName) - 1U; ++i) {
        folderSave[folderBlock + kFolderMetadataOffsetInBlock + i * 2U] = 0U;
        folderSave[folderBlock + kFolderMetadataOffsetInBlock + i * 2U + 1U] =
            static_cast<std::uint8_t>(folderName[i]);
    }
    TitlesSorterSession folderSession;
    assert(folderSession.LoadSave(folderSave, error));
    TitlesSorterUiController folderUi(folderSession);
    folderUi.HandleInput(UiInput::Zr);
    folderUi.Update();
    assert(folderSession.CurrentScreen() == Screen::Preview);
    assert(folderUi.BuildFrame().preview.slots[0].kind == ItemKind::Folder);
    assert(folderUi.BuildFrame().previewSelected);
    folderUi.HandleInput(UiInput::Confirm);
    UiFrame folderFrame = folderUi.BuildFrame();
    assert(folderFrame.insideFolder);
    assert(folderFrame.previewLocation == "Folder");
    assert(folderFrame.preview.slots[0].occupied);
    assert(folderFrame.preview.slots[0].titleId == childTitleId);
    folderUi.HandleInput(UiInput::L); // Folder pages wrap independently (4 pages).
    assert(folderSession.PreviewPageNumber() == 4U);
    folderUi.HandleInput(UiInput::R);
    assert(folderSession.PreviewPageNumber() == 1U);
    folderUi.HandleInput(UiInput::Back);
    assert(!folderUi.BuildFrame().insideFolder);

    return 0;
}

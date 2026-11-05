#include "ui/TitlesSorterUI.hpp"

#include <algorithm>
#include <array>
#include <sstream>
#include <utility>
#include <cctype>

namespace titles_sorter {
namespace {

constexpr std::uint32_t kMaximumExperimentalPage =
    static_cast<std::uint32_t>(kMainSlotCount / kPageCapacity);

constexpr std::array<SortField, 4> kSortFields{
    SortField::TitleId,
    SortField::ProductCode,
    SortField::LongName,
    SortField::ShortName
};

std::string SortFieldLabel(SortField field) {
    switch (field) {
        case SortField::TitleId: return "Title ID";
        case SortField::ProductCode: return "Product Code";
        case SortField::LongName: return "Long Name";
        case SortField::ShortName: return "Short Name";
    }
    return "Unknown";
}

std::string FolderPlacementLabel(FolderPlacement placement) {
    return placement == FolderPlacement::FoldersFirst ? "Folders First" : "Mixed Alphabetically";
}

} // namespace

TitlesSorterUiController::TitlesSorterUiController(TitlesSorterSession& session) noexcept
    : session_(session) {}

void TitlesSorterUiController::HandleInput(UiInput input) {
    lastMessage_.clear();
    switch (session_.CurrentScreen()) {
        case Screen::Settings:
            switch (input) {
                case UiInput::Up: {
                    const auto current = static_cast<unsigned>(focusedRow_);
                    const auto count = static_cast<unsigned>(SettingsRow::Count);
                    focusedRow_ = static_cast<SettingsRow>((current + count - 1U) % count);
                    break;
                }
                case UiInput::Down: {
                    const auto current = static_cast<unsigned>(focusedRow_);
                    const auto count = static_cast<unsigned>(SettingsRow::Count);
                    focusedRow_ = static_cast<SettingsRow>((current + 1U) % count);
                    break;
                }
                case UiInput::Left: ChangeSelectedValue(-1); break;
                case UiInput::Right: ChangeSelectedValue(1); break;
                case UiInput::Confirm:
                    if (focusedRow_ == SettingsRow::SortFolderContents) {
                        session_.Options().sortFolderContents = !session_.Options().sortFolderContents;
                    } else if (focusedRow_ == SettingsRow::ExcludedPages) {
                        excludedPagesInputRequested_ = true;
                    }
                    break;
                case UiInput::Zl:
                case UiInput::Zr: {
                    std::string error;
                    if (!session_.BeginPreview(error)) lastMessage_ = std::move(error);
                    else { openFolderId_ = 0U; selectedPreviewSlot_ = 0U; previewSelected_ = true; }
                    break;
                }
                case UiInput::Back:
                case UiInput::L:
                case UiInput::R:
                    break;
            }
            break;

        case Screen::LoadingData:
        case Screen::GeneratingPreview:
            // Input is intentionally ignored during incremental loading/planning.
            break;

        case Screen::Preview:
            switch (input) {
                case UiInput::Zl:
                case UiInput::Zr: session_.ReturnToSettings(); previewSelected_ = false; openFolderId_ = 0U; break;
                case UiInput::Up: selectedPreviewSlot_ = static_cast<std::uint8_t>((selectedPreviewSlot_ + 10U) % 15U); previewSelected_ = true; break;
                case UiInput::Down: selectedPreviewSlot_ = static_cast<std::uint8_t>((selectedPreviewSlot_ + 5U) % 15U); previewSelected_ = true; break;
                case UiInput::Left: selectedPreviewSlot_ = static_cast<std::uint8_t>((selectedPreviewSlot_ + 14U) % 15U); previewSelected_ = true; break;
                case UiInput::Right: selectedPreviewSlot_ = static_cast<std::uint8_t>((selectedPreviewSlot_ + 1U) % 15U); previewSelected_ = true; break;
                case UiInput::L: session_.PreviousPreviewPage(openFolderId_); selectedPreviewSlot_ = 0; break;
                case UiInput::R: session_.NextPreviewPage(openFolderId_); selectedPreviewSlot_ = 0; break;
                case UiInput::Confirm: {
                    const PreviewPage page = session_.GetPreviewPage(openFolderId_);
                    const PreviewItem& selected = page.slots[selectedPreviewSlot_];
                    if (selected.occupied && selected.kind == ItemKind::Folder && openFolderId_ == 0U &&
                        selected.folderId > 0U) {
                        const SaveModel* model = session_.Model();
                        if (model != nullptr && selected.folderId <= model->folders.size() &&
                            model->folders[selected.folderId - 1U].active) {
                            homePreviewPage_ = session_.PreviewPageNumber();
                            homePreviewSelectedSlot_ = selectedPreviewSlot_;
                            openFolderId_ = selected.folderId;
                            session_.ResetPreviewPage();
                            selectedPreviewSlot_ = 0U;
                            previewSelected_ = true;
                        }
                    }
                    break;
                }
                case UiInput::Back:
                    if (openFolderId_ != 0U) {
                        openFolderId_ = 0U;
                        selectedPreviewSlot_ = homePreviewSelectedSlot_;
                        previewSelected_ = true;
                        session_.SetPreviewPage(homePreviewPage_);
                    }
                    break;
                default: break;
            }
            break;

        case Screen::Error:
            if (input == UiInput::Back || input == UiInput::Confirm)
                session_.ReturnToSettings();
            break;
    }
}

void TitlesSorterUiController::Update() {
    session_.Update();
    if (session_.CurrentScreen() == Screen::Error)
        lastMessage_ = session_.ErrorMessage();
}

bool TitlesSorterUiController::ConsumeExcludedPagesInputRequest() noexcept {
    const bool requested = excludedPagesInputRequested_;
    excludedPagesInputRequested_ = false;
    return requested;
}

void TitlesSorterUiController::CancelExcludedPagesInputRequest() noexcept {
    excludedPagesInputRequested_ = false;
}

bool TitlesSorterUiController::SetExcludedPagesExpression(std::string_view expression,
                                                            std::string& error) {
    std::string normalized(expression);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
        [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (normalized == "none") normalized.clear();
    auto parsed = ParseExcludedPages(normalized, kMaximumExperimentalPage, error);
    if (!parsed.has_value()) { lastMessage_ = error; return false; }
    lastMessage_.clear();
    session_.Options().excludedPages = std::move(*parsed);
    excludedPagesInputRequested_ = false;
    error.clear();
    return true;
}

UiFrame TitlesSorterUiController::BuildFrame() const {
    UiFrame frame;
    frame.screen = session_.CurrentScreen();
    frame.message = !lastMessage_.empty() ? lastMessage_ : session_.ErrorMessage();
    frame.excludedPagesInputRequested = excludedPagesInputRequested_;
    frame.previewPageCount = session_.PreviewPageCount(openFolderId_);
    frame.insideFolder = openFolderId_ != 0U;
    frame.previewLocation = openFolderId_ != 0U ? session_.FolderName(openFolderId_) : "Home Menu";
    frame.previewSelected = previewSelected_;
    frame.selectedPreviewSlot = selectedPreviewSlot_;

    const SortOptions& options = session_.Options();
    frame.settings[0] = {"Sort By", SortFieldLabel(options.field),
                         focusedRow_ == SettingsRow::SortField, true};
    frame.settings[1] = {"Folder Placement", FolderPlacementLabel(options.folderPlacement),
                         focusedRow_ == SettingsRow::FolderPlacement, true};
    frame.settings[2] = {"Sort Folder Contents",
                         options.sortFolderContents ? "ON" : "OFF",
                         focusedRow_ == SettingsRow::SortFolderContents, true};
    frame.settings[3] = {"Excluded Pages", ExcludedPagesExpression(),
                         focusedRow_ == SettingsRow::ExcludedPages, true};

    switch (frame.screen) {
        case Screen::Settings:
            frame.footer = "Navigate Options   Change Options   HOME: Exit   Edit Option";
            break;
        case Screen::LoadingData:
            frame.footer = "Loading Home Menu data...";
            break;
        case Screen::GeneratingPreview:
            frame.footer = "Generating Preview...";
            break;
        case Screen::Preview:
            frame.preview = session_.GetPreviewPage(openFolderId_);
            frame.footer = openFolderId_ == 0U
                ? "Navigate Titles   L: Previous Page   R: Next Page   HOME: Exit   A: Enter Folder"
                : "Navigate Titles   L: Previous Page   R: Next Page   HOME: Exit   B: Back to Home Menu";
            break;
        case Screen::Error:
            frame.footer = "A/B: Return   HOME: System Menu";
            break;
    }
    return frame;
}

SettingsRow TitlesSorterUiController::FocusedSettingsRow() const noexcept { return focusedRow_; }
bool TitlesSorterUiController::IsPreviewSelected() const noexcept { return previewSelected_; }

void TitlesSorterUiController::ChangeSelectedValue(int direction) {
    SortOptions& options = session_.Options();
    switch (focusedRow_) {
        case SettingsRow::SortField: {
            auto it = std::find(kSortFields.begin(), kSortFields.end(), options.field);
            std::size_t index = it == kSortFields.end() ? 0U :
                static_cast<std::size_t>(std::distance(kSortFields.begin(), it));
            const auto count = static_cast<int>(kSortFields.size());
            int next = (static_cast<int>(index) + direction) % count;
            if (next < 0) next += count;
            options.field = kSortFields[static_cast<std::size_t>(next)];
            break;
        }
        case SettingsRow::FolderPlacement:
            options.folderPlacement = options.folderPlacement == FolderPlacement::FoldersFirst
                ? FolderPlacement::MixedAlphabetically : FolderPlacement::FoldersFirst;
            break;
        case SettingsRow::SortFolderContents:
            options.sortFolderContents = !options.sortFolderContents;
            break;
        case SettingsRow::ExcludedPages:
        case SettingsRow::Count:
            excludedPagesInputRequested_ = true;
            break;
    }
}

std::string TitlesSorterUiController::ExcludedPagesExpression() const {
    const auto& pages = session_.Options().excludedPages;
    if (pages.empty()) return "None";
    std::ostringstream stream;
    for (std::size_t i = 0; i < pages.size(); ++i) {
        if (i != 0U) stream << ',';
        stream << pages[i];
    }
    return stream.str();
}

} // namespace titles_sorter

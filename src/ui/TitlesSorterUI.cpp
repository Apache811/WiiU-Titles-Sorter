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
    if (session_.CurrentScreen() == Screen::LoadingData ||
        session_.CurrentScreen() == Screen::GeneratingPreview) return;

    if (session_.CurrentScreen() == Screen::Preview) {
        switch (input) {
            case UiInput::Zl:
                session_.ReturnToSettings();
                section_ = UiSection::Options;
                previewSelected_ = false;
                openFolderId_ = 0U;
                break;
            case UiInput::Zr:
                session_.ReturnToSettings();
                section_ = UiSection::Apply;
                previewSelected_ = false;
                openFolderId_ = 0U;
                break;
            case UiInput::Up:
                selectedPreviewSlot_ = static_cast<std::uint8_t>((selectedPreviewSlot_ + 10U) % 15U);
                previewSelected_ = true;
                break;
            case UiInput::Down:
                selectedPreviewSlot_ = static_cast<std::uint8_t>((selectedPreviewSlot_ + 5U) % 15U);
                previewSelected_ = true;
                break;
            case UiInput::Left:
                selectedPreviewSlot_ = static_cast<std::uint8_t>((selectedPreviewSlot_ + 14U) % 15U);
                previewSelected_ = true;
                break;
            case UiInput::Right:
                selectedPreviewSlot_ = static_cast<std::uint8_t>((selectedPreviewSlot_ + 1U) % 15U);
                previewSelected_ = true;
                break;
            case UiInput::L:
                session_.PreviousPreviewPage(openFolderId_);
                selectedPreviewSlot_ = 0U;
                break;
            case UiInput::R:
                session_.NextPreviewPage(openFolderId_);
                selectedPreviewSlot_ = 0U;
                break;
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
        }
        return;
    }

    if (session_.CurrentScreen() == Screen::Error) {
        if (input == UiInput::Back || input == UiInput::Confirm)
            session_.ReturnToSettings();
        return;
    }

    // Operation result frames accept only A. HOME remains owned by WHBProc.
    if (section_ == UiSection::Apply &&
        (applyConfirmation_ == ApplyConfirmation::BackupResult ||
         applyConfirmation_ == ApplyConfirmation::RestoreResult ||
         applyConfirmation_ == ApplyConfirmation::ApplyResult ||
         applyConfirmation_ == ApplyConfirmation::PreviewRequired)) {
        if (input == UiInput::Confirm) {
            if (applyConfirmation_ == ApplyConfirmation::RestoreResult ||
                applyConfirmation_ == ApplyConfirmation::ApplyResult)
                sessionReloadRequested_ = true;
            applyConfirmation_ = ApplyConfirmation::None;
            lastMessage_.clear();
            dialogDetail_.clear();
        }
        return;
    }

    // Decision frames are modal: only A confirms and B cancels.
    if (section_ == UiSection::Apply && applyConfirmation_ != ApplyConfirmation::None) {
        if (input == UiInput::Confirm) {
            if (applyConfirmation_ == ApplyConfirmation::RestoreDecision)
                restoreRequested_ = true;
            else if (applyConfirmation_ == ApplyConfirmation::ApplyLayoutDecision)
                applyRequested_ = true;
            applyConfirmation_ = ApplyConfirmation::None;
        } else if (input == UiInput::Back) {
            if (applyConfirmation_ == ApplyConfirmation::RestoreDecision)
                showingBackupList_ = true;
            applyConfirmation_ = ApplyConfirmation::None;
        }
        return;
    }

    lastMessage_.clear();

    // Shoulder navigation is deliberately explicit: the three destinations
    // form a cycle, while entering Preview still generates it immediately.
    if (input == UiInput::Zl || input == UiInput::Zr) {
        applyConfirmation_ = ApplyConfirmation::None;
        showingBackupList_ = false;
        requestedBackupPath_.clear();
        if (section_ == UiSection::Options && input == UiInput::Zl) {
            section_ = UiSection::Apply;
        } else if (section_ == UiSection::Options && input == UiInput::Zr) {
            std::string error;
            if (!session_.BeginPreview(error)) {
                lastMessage_ = std::move(error);
            } else {
                openFolderId_ = 0U;
                selectedPreviewSlot_ = 0U;
                previewSelected_ = true;
            }
        } else if (section_ == UiSection::Apply && input == UiInput::Zl) {
            std::string error;
            if (!session_.BeginPreview(error)) {
                lastMessage_ = std::move(error);
            } else {
                openFolderId_ = 0U;
                selectedPreviewSlot_ = 0U;
                previewSelected_ = true;
            }
        } else {
            section_ = UiSection::Options;
        }
        return;
    }

    if (section_ == UiSection::Apply) {
        if (showingBackupList_) {
            if (input == UiInput::Up && !backupNames_.empty())
                selectedBackup_ = (selectedBackup_ + backupNames_.size() - 1U) % backupNames_.size();
            else if (input == UiInput::Down && !backupNames_.empty())
                selectedBackup_ = (selectedBackup_ + 1U) % backupNames_.size();
            else if (input == UiInput::Back) {
                showingBackupList_ = false;
                requestedBackupPath_.clear();
            } else if (input == UiInput::Confirm && !backupNames_.empty()) {
                requestedBackupPath_ = backupNames_[selectedBackup_];
                applyConfirmation_ = ApplyConfirmation::RestoreDecision;
                lastMessage_.clear();
                showingBackupList_ = false;
            }
            return;
        }
        if (input == UiInput::Up) {
            const auto current = static_cast<unsigned>(focusedApplyRow_);
            focusedApplyRow_ = static_cast<ApplyRow>(
                (current + static_cast<unsigned>(ApplyRow::Count) - 1U) % static_cast<unsigned>(ApplyRow::Count));
            requestedBackupPath_.clear();
        } else if (input == UiInput::Down) {
            const auto current = static_cast<unsigned>(focusedApplyRow_);
            focusedApplyRow_ = static_cast<ApplyRow>(
                (current + 1U) % static_cast<unsigned>(ApplyRow::Count));
            requestedBackupPath_.clear();
        } else if (input == UiInput::Left || input == UiInput::Right) {
            if (focusedApplyRow_ == ApplyRow::AutoBackup)
                session_.Options().autoBackup = !session_.Options().autoBackup;
        } else if (input == UiInput::Confirm) {
            switch (focusedApplyRow_) {
                case ApplyRow::AutoBackup:
                    // Autobackup is changed only with D-Pad Left/Right, like Sort By.
                    break;
                case ApplyRow::MaxAutoBackups:
                    autoBackupInputRequested_ = true;
                    break;
                case ApplyRow::CreateBackup:
                    manualBackupRequested_ = true;
                    applyConfirmation_ = ApplyConfirmation::BackupResult;
                    lastMessage_.clear();
                    break;
                case ApplyRow::RestoreBackup:
                    if (!backupNames_.empty()) {
                        showingBackupList_ = true;
                        selectedBackup_ = 0U;
                    } else {
                        lastMessage_ = "No backups are available.";
                    }
                    break;
                case ApplyRow::ApplyLayout:
                    if (session_.Plan() == nullptr) {
                        SetPreviewRequiredMessage();
                    } else {
                        applyConfirmation_ = ApplyConfirmation::ApplyLayoutDecision;
                        lastMessage_.clear();
                    }
                    break;
                case ApplyRow::Count:
                    break;
            }
        } else if (input == UiInput::Back) {
            requestedBackupPath_.clear();
        }
        return;
    }

    // OPTIONS input is intentionally kept identical to the v0.5 controller.
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
        case UiInput::Left:
            ChangeSelectedValue(-1);
            break;
        case UiInput::Right:
            ChangeSelectedValue(1);
            break;
        case UiInput::Confirm:
            if (focusedRow_ == SettingsRow::SortFolderContents) {
                session_.Options().sortFolderContents = !session_.Options().sortFolderContents;
            } else if (focusedRow_ == SettingsRow::ExcludedPages) {
                excludedPagesInputRequested_ = true;
            }
            break;
        case UiInput::Back:
        case UiInput::L:
        case UiInput::R:
        case UiInput::Zl:
        case UiInput::Zr:
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

bool TitlesSorterUiController::SetMaxAutoBackups(std::string_view value, std::string& error) {
    if (value.empty()) { error = "Enter a number between 1 and 100."; return false; }
    std::uint32_t parsed = 0U;
    for (char ch : value) {
        if (ch < '0' || ch > '9') { error = "Only whole numbers are accepted."; return false; }
        const std::uint32_t digit = static_cast<std::uint32_t>(ch - '0');
        if (parsed > 100U || parsed * 10U + digit > 100U) { error = "The maximum is 100 automatic backups."; return false; }
        parsed = parsed * 10U + digit;
    }
    if (parsed == 0U) { error = "The minimum is 1 automatic backup."; return false; }
    session_.Options().maxAutoBackups = parsed;
    autoBackupInputRequested_ = false; error.clear(); return true;
}
bool TitlesSorterUiController::ConsumeAutoBackupInputRequest() noexcept { const bool value = autoBackupInputRequested_; autoBackupInputRequested_ = false; return value; }
void TitlesSorterUiController::CancelAutoBackupInputRequest() noexcept { autoBackupInputRequested_ = false; }
bool TitlesSorterUiController::ConsumeManualBackupRequest() noexcept { const bool value = manualBackupRequested_; manualBackupRequested_ = false; return value; }
bool TitlesSorterUiController::ConsumeRestoreRequest(std::string& backupPath) noexcept {
    if (!restoreRequested_) return false;
    backupPath = std::move(requestedBackupPath_);
    requestedBackupPath_.clear();
    restoreRequested_ = false;
    return true;
}
bool TitlesSorterUiController::ConsumeApplyRequest() noexcept { const bool value = applyRequested_; applyRequested_ = false; return value; }
bool TitlesSorterUiController::ConsumeSessionReloadRequest() noexcept {
    const bool value = sessionReloadRequested_;
    sessionReloadRequested_ = false;
    return value;
}
void TitlesSorterUiController::SetBackups(const std::vector<std::string>& backupNames) { backupNames_ = backupNames; if (selectedBackup_ >= backupNames_.size()) selectedBackup_ = 0U; }
void TitlesSorterUiController::SetOperationMessage(std::string message) { lastMessage_ = std::move(message); }
void TitlesSorterUiController::SetBackupResult(bool succeeded, std::string detail) {
    applyConfirmation_ = ApplyConfirmation::BackupResult;
    lastMessage_ = succeeded ? "Backup Completed !!!" : "Backup Failed !!!";
    dialogDetail_ = std::move(detail);
}
void TitlesSorterUiController::SetRestoreResult(bool succeeded, std::string detail) {
    applyConfirmation_ = ApplyConfirmation::RestoreResult;
    lastMessage_ = succeeded ? "Backup Restored !!!" : "Restore Failed !!!";
    dialogDetail_ = std::move(detail);
}

void TitlesSorterUiController::SetApplyResult(bool succeeded, std::string detail) {
    applyConfirmation_ = ApplyConfirmation::ApplyResult;
    lastMessage_ = succeeded ? "Layout Applied !!!" : "Apply Failed !!!";
    dialogDetail_ = std::move(detail);
}

void TitlesSorterUiController::SetPreviewRequiredMessage() {
    applyConfirmation_ = ApplyConfirmation::PreviewRequired;
    lastMessage_ = "Generate a Preview before applying a layout.";
    dialogDetail_.clear();
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
    frame.autoBackupInputRequested = autoBackupInputRequested_;
    frame.section = section_;
    if (showingBackupList_) frame.backupNames = backupNames_;
    frame.selectedBackup = selectedBackup_;
    frame.showingBackupList = showingBackupList_;
    frame.applyConfirmation = applyConfirmation_;
    frame.dialogDetail = applyConfirmation_ == ApplyConfirmation::RestoreDecision
        ? requestedBackupPath_ : dialogDetail_;
    if (applyConfirmation_ == ApplyConfirmation::RestoreDecision ||
        applyConfirmation_ == ApplyConfirmation::ApplyLayoutDecision)
        frame.footerMode = UiFooterMode::Decision;
    else if (applyConfirmation_ == ApplyConfirmation::BackupResult ||
             applyConfirmation_ == ApplyConfirmation::RestoreResult ||
             applyConfirmation_ == ApplyConfirmation::ApplyResult ||
             applyConfirmation_ == ApplyConfirmation::PreviewRequired)
        frame.footerMode = UiFooterMode::OperationResult;
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
    frame.applyRows[0] = {"Autobackup", options.autoBackup ? "Enabled" : "Disabled", focusedApplyRow_ == ApplyRow::AutoBackup, true};
    frame.applyRows[1] = {"Max Number of Autobackups to Keep", std::to_string(options.maxAutoBackups), focusedApplyRow_ == ApplyRow::MaxAutoBackups, true};
    frame.applyRows[2] = {"Create Backup", "Create a manual backup", focusedApplyRow_ == ApplyRow::CreateBackup, true};
    frame.applyRows[3] = {"Restore Backup", "Select a backup file", focusedApplyRow_ == ApplyRow::RestoreBackup, true};
    frame.applyRows[4] = {"Apply the Previewed Layout", session_.Plan() != nullptr ? "Apply current preview" : "Preview required", focusedApplyRow_ == ApplyRow::ApplyLayout, session_.Plan() != nullptr};

    switch (frame.screen) {
        case Screen::Settings:
            if (section_ == UiSection::Apply) frame.footer = showingBackupList_ ? "Navigate Backups   A: Select   B: Back" : "Navigate Actions   Change Options   HOME: Exit   Select";
            else frame.footer = "Navigate Options   Change Options   HOME: Exit   Edit Option";
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

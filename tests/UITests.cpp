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

    // OPTIONS and PREVIEW retain the v0.5 behaviour. Shoulder navigation:
    // OPTIONS --ZL--> APPLY, OPTIONS --ZR--> immediate Preview generation.
    assert(ui.BuildFrame().footer == "Navigate Options   Change Options   HOME: Exit   Edit Option");
    ui.HandleInput(UiInput::Zl);
    assert(ui.BuildFrame().section == UiSection::Apply);
    ui.HandleInput(UiInput::Zr);
    assert(ui.BuildFrame().section == UiSection::Options);

    ui.HandleInput(UiInput::Zr); // OPTIONS -> PREVIEW; generation starts without A.
    assert(session.CurrentScreen() == Screen::GeneratingPreview);
    ui.Update();
    assert(session.CurrentScreen() == Screen::Preview);
    assert(ui.BuildFrame().previewPageCount == 20U);
    assert(ui.BuildFrame().preview.slots[0].occupied);
    assert(ui.BuildFrame().preview.slots[0].titleId == titleId);

    ui.HandleInput(UiInput::Zl); // PREVIEW -> OPTIONS.
    assert(session.CurrentScreen() == Screen::Settings);
    assert(ui.BuildFrame().section == UiSection::Options);
    ui.HandleInput(UiInput::Zr);
    assert(session.CurrentScreen() == Screen::Preview); // Cached Preview opens immediately.
    ui.HandleInput(UiInput::Zr); // PREVIEW -> APPLY.
    assert(session.CurrentScreen() == Screen::Settings);
    assert(ui.BuildFrame().section == UiSection::Apply);
    ui.HandleInput(UiInput::Zl); // APPLY -> PREVIEW, without A.
    assert(session.CurrentScreen() == Screen::Preview);
    ui.HandleInput(UiInput::Zl); // PREVIEW -> OPTIONS.
    assert(session.CurrentScreen() == Screen::Settings);
    assert(ui.BuildFrame().section == UiSection::Options);

    ui.HandleInput(UiInput::Zr);
    assert(session.CurrentScreen() == Screen::Preview);
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
    assert(session.Plan() != nullptr);
    assert(ui.BuildFrame().section == UiSection::Options);

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
    assert(folderSession.CurrentScreen() == Screen::GeneratingPreview);
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

    // APPLY controls: backup is executed immediately; the result is acknowledged with A only.
    ui.HandleInput(UiInput::Zl); // OPTIONS -> APPLY
    frame = ui.BuildFrame();
    assert(frame.section == UiSection::Apply);
    assert(frame.applyRows[0].value == "Enabled");
    assert(frame.applyRows[1].value == "10");
    ui.HandleInput(UiInput::Confirm); // A does not toggle Autobackup.
    assert(session.Options().autoBackup);
    ui.HandleInput(UiInput::Right);
    assert(!session.Options().autoBackup);
    ui.HandleInput(UiInput::Left);
    assert(session.Options().autoBackup);
    ui.HandleInput(UiInput::Down);
    ui.HandleInput(UiInput::Confirm);
    assert(ui.ConsumeAutoBackupInputRequest());
    assert(ui.SetMaxAutoBackups("15", error));
    assert(session.Options().maxAutoBackups == 15U);
    assert(!ui.SetMaxAutoBackups("0", error));
    assert(session.Options().maxAutoBackups == 15U);
    assert(!ui.SetMaxAutoBackups("101", error));
    assert(session.Options().maxAutoBackups == 15U);

    ui.HandleInput(UiInput::Down); // Create Backup
    ui.HandleInput(UiInput::Confirm);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::BackupResult);
    assert(ui.ConsumeManualBackupRequest()); // The operation starts on the first A.
    ui.HandleInput(UiInput::Back); // B is ignored on result frames.
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::BackupResult);
    ui.SetBackupResult(true, "/backups/backup_2026-11-05_10-00-00.dat");
    frame = ui.BuildFrame();
    assert(frame.message == "Backup Completed !!!");
    assert(frame.dialogDetail == "/backups/backup_2026-11-05_10-00-00.dat");
    assert(frame.footerMode == UiFooterMode::OperationResult);
    ui.HandleInput(UiInput::Back);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::BackupResult);
    assert(ui.BuildFrame().message == "Backup Completed !!!");
    ui.HandleInput(UiInput::Down);
    ui.HandleInput(UiInput::Zl);
    assert(ui.BuildFrame().message == "Backup Completed !!!");
    ui.HandleInput(UiInput::Confirm);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::None);
    ui.SetBackupResult(false, "Unable to create the backup file.");
    assert(ui.BuildFrame().message == "Backup Failed !!!");
    assert(ui.BuildFrame().dialogDetail == "Unable to create the backup file.");
    ui.HandleInput(UiInput::Confirm);

    const std::vector<std::string> backups{ "/backups/backup_2026-11-05_10-00-00.dat" };
    ui.SetBackups(backups);
    ui.HandleInput(UiInput::Down); // Restore Backup
    ui.HandleInput(UiInput::Confirm); // Open selector.
    assert(ui.BuildFrame().showingBackupList);
    ui.HandleInput(UiInput::Confirm); // Select backup and show decision frame.
    frame = ui.BuildFrame();
    assert(frame.applyConfirmation == ApplyConfirmation::RestoreDecision);
    assert(frame.dialogDetail == backups[0]);
    assert(frame.footerMode == UiFooterMode::Decision);
    ui.HandleInput(UiInput::Back); // B returns to the backup selector.
    assert(ui.BuildFrame().showingBackupList);
    ui.HandleInput(UiInput::Confirm); // Select again.
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::RestoreDecision);
    ui.HandleInput(UiInput::Confirm); // A confirms and starts Restore.
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::None);
    assert(ui.ConsumeRestoreRequest(error));
    assert(error == backups[0]);
    ui.SetRestoreResult(true, "Autobackup created: /backups/autobackup_test.dat");
    assert(ui.BuildFrame().message == "Backup Restored !!!");
    assert(ui.BuildFrame().dialogDetail == "Autobackup created: /backups/autobackup_test.dat");
    ui.HandleInput(UiInput::Back); // B is ignored on result frames.
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::RestoreResult);
    assert(ui.BuildFrame().message == "Backup Restored !!!");
    ui.HandleInput(UiInput::Down);
    ui.HandleInput(UiInput::Zr);
    assert(ui.BuildFrame().message == "Backup Restored !!!");
    ui.HandleInput(UiInput::Confirm);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::None);
    assert(ui.ConsumeSessionReloadRequest());
    ui.SetRestoreResult(false, "The selected backup is invalid.");
    assert(ui.BuildFrame().message == "Restore Failed !!!");
    assert(ui.BuildFrame().dialogDetail == "The selected backup is invalid.");
    assert(!ui.ConsumeSessionReloadRequest());
    ui.HandleInput(UiInput::Confirm);
    assert(ui.ConsumeSessionReloadRequest());

    // Apply requires a preview decision; B cancels and A proceeds.
    ui.HandleInput(UiInput::Up); // Restore Backup -> Create Backup
    ui.HandleInput(UiInput::Up); // Max autobackups
    ui.HandleInput(UiInput::Up); // Autobackup
    ui.HandleInput(UiInput::Up); // Apply Layout (wrap)
    ui.HandleInput(UiInput::Confirm);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::ApplyLayoutDecision);
    assert(ui.BuildFrame().footerMode == UiFooterMode::Decision);
    assert(!ui.ConsumeApplyRequest());
    ui.HandleInput(UiInput::Back);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::None);
    assert(!ui.ConsumeApplyRequest());
    ui.HandleInput(UiInput::Confirm);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::ApplyLayoutDecision);
    ui.HandleInput(UiInput::Confirm);
    assert(ui.ConsumeApplyRequest());

    // Apply result is shown before the active save and metadata are reloaded.
    ui.SetApplyResult(true, "Autobackup created: /backups/autobackup_test.dat");
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::ApplyResult);
    assert(ui.BuildFrame().footerMode == UiFooterMode::OperationResult);
    assert(ui.BuildFrame().message == "Layout Applied !!!");
    assert(ui.BuildFrame().dialogDetail.find("Autobackup created:") != std::string::npos);
    ui.HandleInput(UiInput::Back); // B is ignored on result frames.
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::ApplyResult);
    assert(!ui.ConsumeSessionReloadRequest());
    ui.HandleInput(UiInput::Confirm);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::None);
    assert(ui.ConsumeSessionReloadRequest());

    ui.SetApplyResult(false, "Apply transaction failed.");
    assert(ui.BuildFrame().message == "Apply Failed !!!");
    assert(ui.BuildFrame().dialogDetail == "Apply transaction failed.");
    ui.HandleInput(UiInput::Confirm);
    assert(ui.ConsumeSessionReloadRequest());

    session.Options().field = SortField::ProductCode;
    assert(session.Plan() == nullptr);
    assert(!ui.BuildFrame().applyRows[4].interactive);
    ui.HandleInput(UiInput::Confirm);
    frame = ui.BuildFrame();
    assert(frame.applyConfirmation == ApplyConfirmation::PreviewRequired);
    assert(frame.message == "Generate a Preview before applying a layout.");
    assert(frame.footerMode == UiFooterMode::OperationResult);
    ui.HandleInput(UiInput::Back);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::PreviewRequired);
    ui.HandleInput(UiInput::Confirm);
    assert(ui.BuildFrame().applyConfirmation == ApplyConfirmation::None);
    assert(!ui.ConsumeSessionReloadRequest());

    return 0;
}

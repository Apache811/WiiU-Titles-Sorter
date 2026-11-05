#pragma once

#include "core/TitlesSorterSession.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace titles_sorter {

enum class UiInput : std::uint8_t { Up, Down, Left, Right, Confirm, Back, Zl, Zr, L, R };
enum class UiSection : std::uint8_t { Apply, Options };
enum class ApplyRow : std::uint8_t { AutoBackup, MaxAutoBackups, CreateBackup, RestoreBackup, ApplyLayout, Count };
enum class ApplyConfirmation : std::uint8_t {
    None,
    RestoreDecision,
    ApplyLayoutDecision,
    BackupResult,
    RestoreResult,
    ApplyResult,
    PreviewRequired
};
enum class UiFooterMode : std::uint8_t { Standard, Decision, OperationResult };
enum class SettingsRow : std::uint8_t { SortField, FolderPlacement, SortFolderContents, ExcludedPages, Count };

struct SettingsRowView {
    std::string_view label;
    std::string value;
    bool selected{};
    bool editable{};
};

struct ApplyRowView {
    std::string_view label;
    std::string value;
    bool selected{};
    bool interactive{};
};

struct UiFrame {
    Screen screen{Screen::Settings};
    std::array<SettingsRowView, static_cast<std::size_t>(SettingsRow::Count)> settings{};
    std::array<ApplyRowView, static_cast<std::size_t>(ApplyRow::Count)> applyRows{};
    UiSection section{UiSection::Options};
    std::vector<std::string> backupNames;
    std::size_t selectedBackup{};
    bool showingBackupList{};
    ApplyConfirmation applyConfirmation{ApplyConfirmation::None};
    UiFooterMode footerMode{UiFooterMode::Standard};
    std::string dialogDetail;
    PreviewPage preview{};
    std::string message;
    std::string footer;
    std::uint32_t previewPageCount{};
    bool excludedPagesInputRequested{};
    bool autoBackupInputRequested{};
    bool previewSelected{};
    std::uint8_t previewSelectedSlot{};
    std::uint8_t selectedPreviewSlot{};
    bool insideFolder{};
    std::string previewLocation;
};

class TitlesSorterUiController {
public:
    explicit TitlesSorterUiController(TitlesSorterSession& session) noexcept;
    void HandleInput(UiInput input);
    void Update();

    bool SetExcludedPagesExpression(std::string_view expression, std::string& error);
    bool ConsumeExcludedPagesInputRequest() noexcept;
    void CancelExcludedPagesInputRequest() noexcept;
    bool SetMaxAutoBackups(std::string_view value, std::string& error);
    bool ConsumeAutoBackupInputRequest() noexcept;
    void CancelAutoBackupInputRequest() noexcept;
    bool ConsumeManualBackupRequest() noexcept;
    bool ConsumeRestoreRequest(std::string& backupPath) noexcept;
    bool ConsumeApplyRequest() noexcept;
    bool ConsumeSessionReloadRequest() noexcept;
    void SetBackups(const std::vector<std::string>& backupNames);
    void SetOperationMessage(std::string message);
    void SetBackupResult(bool succeeded, std::string detail);
    void SetRestoreResult(bool succeeded, std::string detail);
    void SetApplyResult(bool succeeded, std::string detail);
    void SetPreviewRequiredMessage();

    [[nodiscard]] UiFrame BuildFrame() const;
    [[nodiscard]] SettingsRow FocusedSettingsRow() const noexcept;
    [[nodiscard]] bool IsPreviewSelected() const noexcept;

private:
    void ChangeSelectedValue(int direction);
    [[nodiscard]] std::string ExcludedPagesExpression() const;

    TitlesSorterSession& session_;
    SettingsRow focusedRow_{SettingsRow::SortField};
    ApplyRow focusedApplyRow_{ApplyRow::AutoBackup};
    UiSection section_{UiSection::Options};
    std::vector<std::string> backupNames_;
    std::size_t selectedBackup_{};
    bool showingBackupList_{};
    ApplyConfirmation applyConfirmation_{ApplyConfirmation::None};
    bool autoBackupInputRequested_{};
    bool manualBackupRequested_{};
    bool restoreRequested_{};
    bool applyRequested_{};
    bool sessionReloadRequested_{};
    std::string requestedBackupPath_;
    std::string dialogDetail_;
    bool excludedPagesInputRequested_{};
    std::string lastMessage_;
    std::uint8_t selectedPreviewSlot_{};
    bool previewSelected_{};
    std::uint32_t openFolderId_{};
    std::uint32_t homePreviewPage_{1U};
    std::uint8_t homePreviewSelectedSlot_{};
};

} // namespace titles_sorter

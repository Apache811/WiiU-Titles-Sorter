#pragma once

#include "core/TitlesSorterSession.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace titles_sorter {

enum class UiInput : std::uint8_t {
    Up,
    Down,
    Left,
    Right,
    Confirm,
    Back,
    Zl,
    Zr,
    L,
    R
};

enum class SettingsRow : std::uint8_t {
    SortField,
    FolderPlacement,
    SortFolderContents,
    ExcludedPages,
    Count
};

struct SettingsRowView {
    std::string_view label;
    std::string value;
    bool selected{};
    bool editable{};
};

struct UiFrame {
    Screen screen{Screen::Settings};
    std::array<SettingsRowView, static_cast<std::size_t>(SettingsRow::Count)> settings{};
    PreviewPage preview{};
    std::string message;
    std::string footer;
    std::uint32_t previewPageCount{};
    bool excludedPagesInputRequested{};
    bool previewSelected{};
    std::uint8_t previewSelectedSlot{};
    std::uint8_t selectedPreviewSlot{};
    bool insideFolder{};
    std::string previewLocation;
};

// Platform-independent UI controller. SDL rendering and VPAD polling are kept
// in the Wii U presentation layer; this class owns only navigation semantics.
class TitlesSorterUiController {
public:
    explicit TitlesSorterUiController(TitlesSorterSession& session) noexcept;

    void HandleInput(UiInput input);
    void Update();

    // Called by a text-entry widget after the user edits the excluded-page
    // expression. Invalid expressions are rejected without changing options.
    bool SetExcludedPagesExpression(std::string_view expression, std::string& error);
    bool ConsumeExcludedPagesInputRequest() noexcept;
    void CancelExcludedPagesInputRequest() noexcept;

    [[nodiscard]] UiFrame BuildFrame() const;
    [[nodiscard]] SettingsRow FocusedSettingsRow() const noexcept;
    [[nodiscard]] bool IsPreviewSelected() const noexcept;

private:
    void ChangeSelectedValue(int direction);
    [[nodiscard]] std::string ExcludedPagesExpression() const;

    TitlesSorterSession& session_;
    SettingsRow focusedRow_{SettingsRow::SortField};
    bool excludedPagesInputRequested_{};
    std::string lastMessage_;
    std::uint8_t selectedPreviewSlot_{};
    bool previewSelected_{};
    std::uint32_t openFolderId_{};
    std::uint32_t homePreviewPage_{1U};
    std::uint8_t homePreviewSelectedSlot_{};
};

} // namespace titles_sorter

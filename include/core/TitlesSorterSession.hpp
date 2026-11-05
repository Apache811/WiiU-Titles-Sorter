#pragma once

#include "core/TitlesSorterCore.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <memory>
#include <string>
#include <vector>

namespace titles_sorter {

enum class Screen : std::uint8_t {
    Settings,
    LoadingData,
    GeneratingPreview,
    Preview,
    Error
};

struct PreviewItem {
    ItemKind kind{ItemKind::Empty};
    std::uint64_t titleId{};
    std::uint32_t folderId{};
    FolderColor folderColor{FolderColor::Blue};
    std::string label;
    std::string iconPath;
    std::shared_ptr<const std::vector<std::uint8_t>> iconData;
    bool occupied{};
    bool pinned{};
};

struct PreviewPage {
    std::uint32_t pageNumber{}; // 1-based
    std::array<PreviewItem, kPageCapacity> slots{};
};

// Application-level state machine. It deliberately owns no SDL objects and
// performs no save writes. The UI can render Settings/GeneratingPreview/Preview
// from this state and feed controller actions back into the session.
class TitlesSorterSession {
public:
    bool LoadSave(std::span<const std::uint8_t> bytes, std::string& error);
    void SetMetadata(const MetadataCatalog* metadata) noexcept;

    [[nodiscard]] SortOptions& Options() noexcept;
    [[nodiscard]] const SortOptions& Options() const noexcept;
    [[nodiscard]] Screen CurrentScreen() const noexcept;
    [[nodiscard]] const std::string& ErrorMessage() const noexcept;
    [[nodiscard]] const SaveModel* Model() const noexcept;
    [[nodiscard]] const PlanResult* Plan() const noexcept;

    // ZR from Settings: show the generation state for at least one UI frame.
    bool BeginPreview(std::string& error);
    // Call once per main-loop iteration while CurrentScreen()==GeneratingPreview.
    void Update();
    // B from Preview: return to Settings without changing the save/model.
    void ReturnToSettings() noexcept;

    void PreviousPreviewPage(std::uint32_t folderId = 0U) noexcept;
    void NextPreviewPage(std::uint32_t folderId = 0U) noexcept;
    void ResetPreviewPage() noexcept;
    void SetPreviewPage(std::uint32_t page) noexcept;
    [[nodiscard]] std::uint32_t PreviewPageNumber() const noexcept;
    [[nodiscard]] std::uint32_t PreviewPageCount() const noexcept;
    [[nodiscard]] PreviewPage GetPreviewPage(std::uint32_t folderId = 0U) const;
    [[nodiscard]] std::uint32_t PreviewPageCount(std::uint32_t folderId) const noexcept;
    [[nodiscard]] std::string FolderName(std::uint32_t folderId) const;

private:
    Screen screen_{Screen::Settings};
    SortOptions options_{};
    std::unique_ptr<SaveModel> model_;
    bool hasModel_{};
    const MetadataCatalog* metadata_{};
    PlanResult plan_{};
    bool hasPlan_{};
    SortOptions plannedOptions_{};
    const MetadataCatalog* plannedMetadata_{};
    std::uint32_t previewPage_{1};
    std::string errorMessage_;
};

} // namespace titles_sorter

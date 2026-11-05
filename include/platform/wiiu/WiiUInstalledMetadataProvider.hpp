#pragma once

#include "core/TitlesSorterCore.hpp"

#include <cstddef>
#include <string>
#include <span>
#include <cstdint>
#include <vector>
#include <unordered_map>

namespace titles_sorter {

enum class MetadataLoadState : std::uint8_t {
    Idle,
    Loading,
    Completed,
    Failed
};

struct MetadataLoadProgress {
    MetadataLoadState state{MetadataLoadState::Idle};
    std::size_t processed{};
    std::size_t total{};
    std::size_t catalogEntries{};
    std::size_t warningCount{};
};

// Wii U implementation backed by MCP and the installed title's own
// meta/meta.xml. Call LoadStep() once per frame to keep the UI responsive.
// This class is implemented in platform/wiiu and requires the WUT headers.
class WiiUInstalledMetadataProvider {
public:
    WiiUInstalledMetadataProvider() = default;
    WiiUInstalledMetadataProvider(const WiiUInstalledMetadataProvider&) = delete;
    WiiUInstalledMetadataProvider& operator=(const WiiUInstalledMetadataProvider&) = delete;

    // Without an ID list, load the standard game/update/DLC title families.
    bool Begin(std::string& error);
    // Load metadata only for title IDs referenced by the current Home Menu save.
    // This includes installed system applications without scanning every installed title.
    bool Begin(std::span<const std::uint64_t> requestedTitleIds, std::string& error);
    bool LoadStep(std::string& error);

    // Resolve a synthetic Homebrew on Menu title by matching its lower ID to
    // the executable-path hash used by the plugin. WUHB metadata/icons are read
    // from SD via libwuhbutils; ACP remains a fallback. A failure is non-fatal
    // for the catalog and is recorded in Warnings().
    bool AddHomebrewTitleMetadata(std::uint64_t titleId, std::string& error);
    void Reset();

    [[nodiscard]] bool IsLoading() const noexcept;
    [[nodiscard]] bool IsComplete() const noexcept;
    [[nodiscard]] MetadataLoadProgress Progress() const noexcept;
    [[nodiscard]] SystemLanguage Language() const noexcept;
    [[nodiscard]] const MetadataCatalog& Catalog() const noexcept;
    [[nodiscard]] const std::vector<std::string>& Warnings() const noexcept;

private:
    bool BeginInternal(std::span<const std::uint64_t> requestedTitleIds,
                       bool restrictToRequestedIds, std::string& error);
    void BuildHomebrewIndex();

    struct PendingTitle {
        std::uint64_t titleId{};
        std::string metaPath;
        std::string iconPath;
    };

    MetadataLoadState state_{MetadataLoadState::Idle};
    SystemLanguage language_{SystemLanguage::English};
    std::vector<PendingTitle> pending_;
    std::size_t nextIndex_{};
    MetadataCatalog catalog_;
    std::vector<std::string> warnings_;
    std::string fatalError_;
    bool homebrewIndexBuilt_{};
    std::unordered_map<std::uint32_t, std::string> homebrewPaths_;
};

} // namespace titles_sorter

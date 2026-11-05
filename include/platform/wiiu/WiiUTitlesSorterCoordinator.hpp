#pragma once

#include "core/TitlesSorterSession.hpp"
#include "platform/wiiu/WiiUInstalledMetadataProvider.hpp"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace titles_sorter {

enum class CoordinatorState : std::uint8_t {
    Idle,
    LoadingNativeMetadata,
    LoadingHomebrewMetadata,
    Ready,
    Failed
};

// Coordinates the read-only startup pipeline on Wii U. The caller supplies
// bytes obtained by the yet-to-be-verified account-save reader; this class does
// not locate, open, modify, or replace the save file itself.
class WiiUTitlesSorterCoordinator {
public:
    WiiUTitlesSorterCoordinator() = default;
    WiiUTitlesSorterCoordinator(const WiiUTitlesSorterCoordinator&) = delete;
    WiiUTitlesSorterCoordinator& operator=(const WiiUTitlesSorterCoordinator&) = delete;

    bool Begin(std::span<const std::uint8_t> saveBytes, std::string& error);
    // Call once per frame. Processes at most one native meta.xml or one ACP
    // Homebrew metadata record per call.
    void Update(std::string& error);
    void Reset();

    [[nodiscard]] CoordinatorState State() const noexcept;
    [[nodiscard]] bool IsLoading() const noexcept;
    [[nodiscard]] bool IsReady() const noexcept;
    [[nodiscard]] const TitlesSorterSession& Session() const noexcept;
    [[nodiscard]] TitlesSorterSession& Session() noexcept;
    [[nodiscard]] const std::vector<std::string>& Warnings() const noexcept;
    [[nodiscard]] std::size_t HomebrewMetadataProcessed() const noexcept;
    [[nodiscard]] std::size_t HomebrewMetadataTotal() const noexcept;

private:
    void CollectHomebrewIds();

    CoordinatorState state_{CoordinatorState::Idle};
    TitlesSorterSession session_;
    WiiUInstalledMetadataProvider metadataProvider_;
    std::vector<std::uint64_t> homebrewIds_;
    std::size_t nextHomebrewIndex_{};
    std::vector<std::string> warnings_;
    std::string fatalError_;
};

} // namespace titles_sorter

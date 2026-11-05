#pragma once

#include "core/WiiUHomeMenuSaveLocator.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace titles_sorter {

inline constexpr std::size_t kExpectedHomeMenuSaveSize = 0x1FC44U;

enum class SaveFileStatus : std::uint8_t {
    Success,
    NotFound,
    IoError,
    TooLarge,
    TooSmall
};

// Platform adapter contract. Implementations must perform read-only operations
// and must never truncate, create, or modify the requested file.
class IReadOnlySaveFileSystem {
public:
    virtual ~IReadOnlySaveFileSystem() = default;
    [[nodiscard]] virtual SaveFileStatus GetFileSize(
        const std::string& path, std::size_t& size) = 0;
    [[nodiscard]] virtual SaveFileStatus ReadExact(
        const std::string& path, std::uint8_t* destination, std::size_t size) = 0;
};

struct WiiUHomeMenuSaveSnapshot {
    WiiUHomeMenuSaveLocation location;
    std::vector<std::uint8_t> bytes;
};

// Reads the active account's save. If the plugin redirected save exists, it is
// authoritative: any size/read error is fatal and MLC fallback is forbidden.
[[nodiscard]] bool ReadActiveHomeMenuSave(
    WiiUHomeMenuSaveIdentity identity,
    IReadOnlySaveFileSystem& fileSystem,
    WiiUHomeMenuSaveSnapshot& snapshot,
    std::string& error);

} // namespace titles_sorter

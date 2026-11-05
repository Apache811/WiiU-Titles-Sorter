#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace titles_sorter {

struct WiiUHomeMenuSaveIdentity {
    // Full Wii U Menu title ID, expected high word 0x00050010.
    std::uint64_t systemMenuTitleId{};
    // ACT persistent ID of the currently active account.
    std::uint32_t userPersistentId{};
    // Console serial identity, formed as code_id followed by serial_id.
    std::string consoleSerialId;
    // True only when the Homebrew On Wii U Menu redirected save exists.
    bool pluginRedirectedSaveExists{};
};

struct WiiUHomeMenuSaveLocation {
    std::string activePath;
    std::string mlcPath;
    std::optional<std::string> pluginRedirectedPath;
    std::uint32_t userDirectoryId{};
    bool usesPluginRedirect{};
};

// Resolves the active account's Home Menu save location without opening or
// modifying any file. The plugin's redirected copy takes precedence whenever
// it exists, because the Wii U Menu may be actively using that copy instead of
// the underlying MLC save.
[[nodiscard]] std::optional<WiiUHomeMenuSaveLocation> ResolveHomeMenuSaveLocation(
    const WiiUHomeMenuSaveIdentity& identity, std::string& error);

} // namespace titles_sorter

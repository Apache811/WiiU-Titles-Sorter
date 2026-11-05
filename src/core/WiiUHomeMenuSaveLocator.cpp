#include "core/WiiUHomeMenuSaveLocator.hpp"

#include <algorithm>
#include <cstdio>
#include <string_view>

namespace titles_sorter {
namespace {
constexpr std::uint64_t kWiiUMenuTitleHigh = 0x00050010ULL;
constexpr std::uint32_t kUserDirectoryFlag = 0x80000000U;

bool IsSafeSerialId(std::string_view value) noexcept {
    if (value.empty() || value.size() > 31U) return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char ch) {
        return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
               (ch >= '0' && ch <= '9') || ch == '-' || ch == '_';
    });
}

} // namespace

std::optional<WiiUHomeMenuSaveLocation> ResolveHomeMenuSaveLocation(
    const WiiUHomeMenuSaveIdentity& identity, std::string& error) {
    error.clear();
    if ((identity.systemMenuTitleId >> 32U) != kWiiUMenuTitleHigh ||
        static_cast<std::uint32_t>(identity.systemMenuTitleId) == 0U) {
        error = "The resolved system application is not a valid Wii U Menu title.";
        return std::nullopt;
    }
    if (identity.userPersistentId == 0U) {
        error = "ACT returned an invalid active user persistent ID.";
        return std::nullopt;
    }

    const std::uint32_t menuTitleLow = static_cast<std::uint32_t>(identity.systemMenuTitleId);
    const std::uint32_t userDirectoryId = kUserDirectoryFlag | identity.userPersistentId;

    char mlcPath[160]{};
    const int mlcLength = std::snprintf(
        mlcPath, sizeof(mlcPath),
        "storage_mlc:/usr/save/00050010/%08x/user/%08x/BaristaAccountSaveFile.dat",
        menuTitleLow, userDirectoryId);
    if (mlcLength < 0 || static_cast<std::size_t>(mlcLength) >= sizeof(mlcPath)) {
        error = "Unable to construct the MLC Home Menu save path.";
        return std::nullopt;
    }

    WiiUHomeMenuSaveLocation result;
    result.mlcPath = mlcPath;
    result.activePath = result.mlcPath;
    result.userDirectoryId = userDirectoryId;

    if (identity.pluginRedirectedSaveExists) {
        if (!IsSafeSerialId(identity.consoleSerialId)) {
            error = "The plugin save exists, but the console serial identity is invalid.";
            return std::nullopt;
        }
        char pluginPath[224]{};
        const int pluginLength = std::snprintf(
            pluginPath, sizeof(pluginPath),
            "fs:/vol/external01/wiiu/homebrew_on_menu_plugin/%s/save/%08x/BaristaAccountSaveFile.dat",
            identity.consoleSerialId.c_str(), userDirectoryId);
        if (pluginLength < 0 || static_cast<std::size_t>(pluginLength) >= sizeof(pluginPath)) {
            error = "Unable to construct the plugin-redirected Home Menu save path.";
            return std::nullopt;
        }
        result.pluginRedirectedPath = pluginPath;
        result.activePath = *result.pluginRedirectedPath;
        result.usesPluginRedirect = true;
    }

    return result;
}

} // namespace titles_sorter

#include "core/TitlesSorterSettingsStore.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace titles_sorter {
namespace {

constexpr std::uint32_t kMaximumExcludedPage =
    static_cast<std::uint32_t>(kMainSlotCount / kPageCapacity);

std::string Serialize(const SortOptions& options) {
    std::string excluded;
    for (std::size_t i = 0; i < options.excludedPages.size(); ++i) {
        if (i != 0U) excluded += ',';
        excluded += std::to_string(options.excludedPages[i]);
    }
    return std::string("version=1\n") +
        "sort_field=" + std::to_string(static_cast<unsigned>(options.field)) + "\n" +
        "folder_placement=" + std::to_string(static_cast<unsigned>(options.folderPlacement)) + "\n" +
        "sort_folder_contents=" + (options.sortFolderContents ? "1\n" : "0\n") +
        "sort_folders_by_contents=" + (options.sortFoldersByContents ? "1\n" : "0\n") +
        "excluded_pages=" + excluded + "\n";
}

bool ParseUnsigned(std::string_view value, unsigned maximum, unsigned& result) {
    if (value.empty()) return false;
    unsigned parsed = 0U;
    for (const char ch : value) {
        if (ch < '0' || ch > '9') return false;
        const unsigned digit = static_cast<unsigned>(ch - '0');
        if (digit > maximum || parsed > (maximum - digit) / 10U) return false;
        parsed = parsed * 10U + digit;
    }
    if (parsed > maximum) return false;
    result = parsed;
    return true;
}

bool ParseContent(std::string_view content, SortOptions& options, std::string& error) {
    SortOptions parsed{};
    bool versionSeen = false;
    bool sortFieldSeen = false;
    bool placementSeen = false;
    bool contentsSeen = false;
    bool foldersContentsSeen = false;
    bool excludedSeen = false;
    std::size_t position = 0U;
    while (position < content.size()) {
        const std::size_t end = content.find('\n', position);
        const std::string_view line = content.substr(position,
            end == std::string_view::npos ? content.size() - position : end - position);
        position = end == std::string_view::npos ? content.size() : end + 1U;
        if (line.empty()) continue;
        const std::size_t separator = line.find('=');
        if (separator == std::string_view::npos) { error = "Malformed settings line."; return false; }
        const std::string_view key = line.substr(0U, separator);
        const std::string_view value = line.substr(separator + 1U);
        unsigned number = 0U;
        if (key == "version") {
            if (versionSeen || !ParseUnsigned(value, 1U, number) || number != 1U) {
                error = "Unsupported settings file version."; return false;
            }
            versionSeen = true;
        } else if (key == "sort_field") {
            if (sortFieldSeen || !ParseUnsigned(value, 3U, number)) { error = "Invalid sort_field setting."; return false; }
            parsed.field = static_cast<SortField>(number); sortFieldSeen = true;
        } else if (key == "folder_placement") {
            if (placementSeen || !ParseUnsigned(value, 1U, number)) { error = "Invalid folder_placement setting."; return false; }
            parsed.folderPlacement = static_cast<FolderPlacement>(number); placementSeen = true;
        } else if (key == "sort_folder_contents") {
            if (contentsSeen || !ParseUnsigned(value, 1U, number)) { error = "Invalid sort_folder_contents setting."; return false; }
            parsed.sortFolderContents = number != 0U; contentsSeen = true;
        } else if (key == "sort_folders_by_contents") {
            if (foldersContentsSeen || !ParseUnsigned(value, 1U, number)) { error = "Invalid sort_folders_by_contents setting."; return false; }
            parsed.sortFoldersByContents = number != 0U; foldersContentsSeen = true;
        } else if (key == "excluded_pages") {
            if (excludedSeen) { error = "Duplicate excluded_pages setting."; return false; }
            std::string parseError;
            auto pages = ParseExcludedPages(value, kMaximumExcludedPage, parseError);
            if (!pages) { error = "Invalid excluded_pages setting: " + parseError; return false; }
            parsed.excludedPages = std::move(*pages); excludedSeen = true;
        } else {
            error = "Unknown settings key."; return false;
        }
    }
    if (!versionSeen || !sortFieldSeen || !placementSeen || !contentsSeen ||
        !foldersContentsSeen || !excludedSeen) {
        error = "Settings file is incomplete."; return false;
    }
    options = std::move(parsed);
    return true;
}

bool WriteFile(const std::string& path, std::string_view content, std::string& error) {
    const std::string temporaryPath = path + ".tmp";
    const std::string backupPath = path + ".bak";
    std::FILE* file = std::fopen(temporaryPath.c_str(), "wb");
    if (file == nullptr) { error = "Unable to open temporary settings file."; return false; }
    const std::size_t written = std::fwrite(content.data(), 1U, content.size(), file);
    const bool flushFailed = std::fflush(file) != 0;
    const int closeResult = std::fclose(file);
    if (written != content.size() || flushFailed || closeResult != 0) {
        std::remove(temporaryPath.c_str());
        error = "Unable to write complete settings file.";
        return false;
    }

    // Keep the previous valid file recoverable while replacing it. This also
    // accommodates FAT implementations where rename cannot overwrite a file.
    std::remove(backupPath.c_str());
    const bool hadPreviousFile = std::rename(path.c_str(), backupPath.c_str()) == 0;
    if (!hadPreviousFile && errno != ENOENT) {
        std::remove(temporaryPath.c_str());
        error = "Unable to preserve the previous settings file.";
        return false;
    }
    if (std::rename(temporaryPath.c_str(), path.c_str()) != 0) {
        if (hadPreviousFile) (void)std::rename(backupPath.c_str(), path.c_str());
        std::remove(temporaryPath.c_str());
        error = "Unable to install the completed settings file.";
        return false;
    }
    if (hadPreviousFile) std::remove(backupPath.c_str());
    return true;
}

} // namespace

bool TitlesSorterSettingsStore::Load(const std::string& path, SortOptions& options,
                                     std::string& error) {
    error.clear();
    options = SortOptions{};
    std::FILE* file = std::fopen(path.c_str(), "rb");
    bool loadedBackup = false;
    const std::string backupPath = path + ".bak";
    if (file == nullptr && (errno == ENOENT || errno == ENOTDIR)) {
        file = std::fopen(backupPath.c_str(), "rb");
        loadedBackup = file != nullptr;
    }
    if (file == nullptr) {
        if (errno == ENOENT || errno == ENOTDIR) {
            lastSavedContent_ = Serialize(options);
            hasKnownContent_ = true;
            return true;
        }
        error = "Unable to open settings file.";
        return false;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file); error = "Unable to inspect settings file."; return false;
    }
    const long length = std::ftell(file);
    if (length < 0 || length > 4096L || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file); error = "Settings file size is invalid."; return false;
    }
    std::string content(static_cast<std::size_t>(length), '\0');
    const std::size_t read = std::fread(content.data(), 1U, content.size(), file);
    const bool failed = read != content.size() || std::ferror(file) != 0;
    const int closeResult = std::fclose(file);
    if (failed || closeResult != 0) { error = "Unable to read settings file."; return false; }
    if (!ParseContent(content, options, error)) {
        options = SortOptions{};
        hasKnownContent_ = false;
        return false;
    }
    if (loadedBackup && std::rename(backupPath.c_str(), path.c_str()) != 0) {
        options = SortOptions{};
        error = "Settings backup was valid but could not be restored to the main file.";
        return false;
    }
    lastSavedContent_ = Serialize(options);
    hasKnownContent_ = true;
    return true;
}

bool TitlesSorterSettingsStore::SaveIfChanged(const std::string& path,
                                              const SortOptions& options,
                                              std::string& error) {
    error.clear();
    const std::string content = Serialize(options);
    if (hasKnownContent_ && content == lastSavedContent_) return true;
    if (!WriteFile(path, content, error)) return false;
    lastSavedContent_ = content;
    hasKnownContent_ = true;
    return true;
}

} // namespace titles_sorter

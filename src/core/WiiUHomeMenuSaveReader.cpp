#include "core/WiiUHomeMenuSaveReader.hpp"

#include <utility>

namespace titles_sorter {
namespace {

const char* DescribeStatus(SaveFileStatus status) noexcept {
    switch (status) {
    case SaveFileStatus::Success: return "success";
    case SaveFileStatus::NotFound: return "file not found";
    case SaveFileStatus::IoError: return "I/O error";
    case SaveFileStatus::TooLarge: return "file exceeds the permitted size";
    case SaveFileStatus::TooSmall: return "file is smaller than the expected size";
    }
    return "unknown filesystem error";
}

bool ReadValidatedFile(
    const std::string& path,
    IReadOnlySaveFileSystem& fileSystem,
    std::vector<std::uint8_t>& bytes,
    std::string& error) {
    std::size_t fileSize = 0U;
    const SaveFileStatus sizeStatus = fileSystem.GetFileSize(path, fileSize);
    if (sizeStatus == SaveFileStatus::TooLarge || sizeStatus == SaveFileStatus::TooSmall) {
        error = "Unsupported Home Menu save size: expected 0x1FC44 bytes, received ";
        error += std::to_string(fileSize);
        error += " bytes. The file was not parsed or modified.";
        return false;
    }
    if (sizeStatus != SaveFileStatus::Success) {
        error = "Unable to inspect active Home Menu save (";
        error += DescribeStatus(sizeStatus);
        error += ").";
        return false;
    }
    if (fileSize != kExpectedHomeMenuSaveSize) {
        error = "Unsupported Home Menu save size: expected 0x1FC44 bytes, received ";
        error += std::to_string(fileSize);
        error += " bytes. The file was not parsed or modified.";
        return false;
    }

    std::vector<std::uint8_t> candidate(fileSize);
    const SaveFileStatus readStatus = fileSystem.ReadExact(path, candidate.data(), candidate.size());
    if (readStatus != SaveFileStatus::Success) {
        error = "Unable to read active Home Menu save (";
        error += DescribeStatus(readStatus);
        error += ").";
        return false;
    }
    bytes = std::move(candidate);
    return true;
}

} // namespace

bool ReadActiveHomeMenuSave(
    WiiUHomeMenuSaveIdentity identity,
    IReadOnlySaveFileSystem& fileSystem,
    WiiUHomeMenuSaveSnapshot& snapshot,
    std::string& error) {
    error.clear();

    // Probe the redirected location before resolving the active path. A hard
    // error is not treated as absence: doing so could select a stale MLC copy.
    if (identity.consoleSerialId.empty()) {
        error = "Console serial identity is unavailable; refusing to assume that plugin save redirection is absent.";
        return false;
    }
    std::string probeError;
    // Resolve once with the redirect flag to validate and construct the
    // candidate path; the existence probe itself is represented by size.
    identity.pluginRedirectedSaveExists = true;
    const auto redirected = ResolveHomeMenuSaveLocation(identity, probeError);
    if (!redirected.has_value()) {
        error = std::move(probeError);
        return false;
    }
    std::size_t ignoredSize = 0U;
    const SaveFileStatus probe = fileSystem.GetFileSize(*redirected->pluginRedirectedPath, ignoredSize);
    if (probe == SaveFileStatus::Success || probe == SaveFileStatus::TooLarge || probe == SaveFileStatus::TooSmall) {
        // Existing file: it is authoritative even if malformed.
        identity.pluginRedirectedSaveExists = true;
    } else if (probe == SaveFileStatus::NotFound) {
        identity.pluginRedirectedSaveExists = false;
    } else {
        error = "Unable to determine whether the plugin-redirected save exists; refusing MLC fallback (I/O error).";
        return false;
    }

    auto location = ResolveHomeMenuSaveLocation(identity, error);
    if (!location.has_value()) return false;

    std::vector<std::uint8_t> candidate;
    if (!ReadValidatedFile(location->activePath, fileSystem, candidate, error)) {
        // Deliberately no fallback here. If activePath is the plugin path, it
        // remains the sole authoritative source until the user resolves it.
        return false;
    }

    WiiUHomeMenuSaveSnapshot result;
    result.location = std::move(*location);
    result.bytes = std::move(candidate);
    snapshot = std::move(result);
    return true;
}

} // namespace titles_sorter

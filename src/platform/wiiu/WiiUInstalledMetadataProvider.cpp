#include "platform/wiiu/WiiUInstalledMetadataProvider.hpp"
#include "core/WiiUMetadataProvider.hpp"

#include <coreinit/mcp.h>
#include <nn/acp.h>
#if defined(__WIIU__)
#include <wuhb_utils/utils.h>
#include <dirent.h>
#include <sys/stat.h>
#endif
#include <malloc.h>

#include <algorithm>
#include <cctype>
#include <memory>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <string_view>
#include <utility>
#include <unordered_set>

namespace titles_sorter {
namespace {

constexpr std::size_t kMaxMetaXmlBytes = 256U * 1024U;
constexpr std::size_t kMaxSystemXmlBytes = 64U * 1024U;
// Protect the console heap from corrupted or unexpected MCP counts. A normal
// Wii U installation is far below this limit; the resulting title-list buffer
// is bounded to roughly 250 KiB.
constexpr std::uint32_t kMaxInstalledTitles = 4096U;
constexpr std::uint64_t kBaseGameHigh = 0x00050000ULL;
constexpr std::uint64_t kVirtualConsoleHigh = 0x00050002ULL;
constexpr std::uint64_t kDlcHigh = 0x0005000CULL;
constexpr std::uint64_t kUpdateHigh = 0x0005000EULL;
constexpr std::uint64_t kHomebrewHigh = 0x0005000FULL;
// WiiUBrew title database: the Disc icon is a nominal ID (Product Code N/A),
// while 10004000 is OSv0/cafe2wii, the system title that launches vWii.
constexpr std::uint64_t kDiscChannelTitleId = 0x0005001010055000ULL;
constexpr std::uint64_t kVwiiLaunchTitleId = 0x0005001010004000ULL;

std::string DiscChannelName(SystemLanguage language) {
    switch (language) {
        case SystemLanguage::Japanese: return "ディスク";
        case SystemLanguage::French: return "Disque";
        case SystemLanguage::German: return "Disc";
        case SystemLanguage::Italian: return "Disco";
        case SystemLanguage::Spanish: return "Disco";
        case SystemLanguage::ChineseSimplified: return "光盘";
        case SystemLanguage::Korean: return "디스크";
        case SystemLanguage::Dutch: return "Disc";
        case SystemLanguage::Portuguese: return "Disco";
        case SystemLanguage::Russian: return "Диск";
        case SystemLanguage::ChineseTraditional: return "光碟";
        case SystemLanguage::English: return "Disc";
    }
    return "Disc";
}

void ApplySpecialChannelMetadata(std::uint64_t titleId, SystemLanguage language,
                                 MetadataCatalog& catalog) {
    if (titleId != kDiscChannelTitleId && titleId != kVwiiLaunchTitleId) return;

    TitleMetadata& metadata = catalog[titleId];
    if (titleId == kDiscChannelTitleId) {
        metadata.longName = DiscChannelName(language);
        metadata.shortName = metadata.longName;
        metadata.productCode.clear(); // WiiUBrew explicitly lists this as N/A.
        if (metadata.iconPath.empty())
            metadata.iconPath = "storage_mlc01:/sys/title/00050010/10055000/meta/iconTex.tga";
    } else {
        metadata.longName = "Wii";
        metadata.shortName = "Wii";
        metadata.productCode.clear(); // No Product Code is listed for OSv0.
        if (metadata.iconPath.empty())
            metadata.iconPath = "storage_mlc01:/sys/title/00050010/10004000/meta/iconTex.tga";
    }
}

// ACP expects its metadata output buffer to be 64-byte aligned. Use the
// platform allocator consistently: UCRT/Windows requires _aligned_malloc and
// _aligned_free, while Wii U (and POSIX hosts) use memalign/free.
void* AllocateAlignedAcpMetadata(std::size_t size) noexcept {
#if defined(_MSC_VER) || defined(_WIN32)
    return _aligned_malloc(size, 0x40U);
#else
    return memalign(0x40U, size);
#endif
}

void FreeAlignedAcpMetadata(void* storage) noexcept {
#if defined(_MSC_VER) || defined(_WIN32)
    _aligned_free(storage);
#else
    std::free(storage);
#endif
}

// Keep the assumptions about MCPTitleListType explicit. A mismatch should
// fail the target build rather than silently reading an incorrect path.
static_assert(offsetof(MCPTitleListType, titleId) == 0x00);
static_assert(offsetof(MCPTitleListType, path) == 0x0C);
static_assert(sizeof(((MCPTitleListType*)nullptr)->path) == 56U);
static_assert(sizeof(MCPTitleListType) == 0x61U);

bool IsSortableTitle(std::uint64_t titleId) noexcept {
    const std::uint64_t high = titleId >> 32U;
    return high == kBaseGameHigh || high == kVirtualConsoleHigh ||
           high == kDlcHigh || high == kUpdateHigh;
}

template <std::size_t N>
std::string CopyFixedString(const char (&value)[N]) {
    const char* end = std::find(value, value + N, '\0');
    return std::string(value, end);
}

std::string DecodeAcpString(const char* value, std::size_t capacity) {
    if (value == nullptr || capacity == 0U) return {};
    const char* end = std::find(value, value + capacity, '\0');
    return DecodeXmlEntities(std::string_view(value, static_cast<std::size_t>(end - value)));
}

void CopyAcpLocalizedMetadata(const ACPMetaXml& source, LocalizedTitleMetadata& target) {
    // ACPGetTitleMetaXml is the same fallback used by Titles Info when the
    // installed title's meta.xml does not provide a usable localized name.
    const std::array<const char*, 12> longNames{
        source.longname_ja, source.longname_en, source.longname_fr, source.longname_de,
        source.longname_it, source.longname_es, source.longname_zhs, source.longname_ko,
        source.longname_nl, source.longname_pt, source.longname_ru, source.longname_zht
    };
    const std::array<const char*, 12> shortNames{
        source.shortname_ja, source.shortname_en, source.shortname_fr, source.shortname_de,
        source.shortname_it, source.shortname_es, source.shortname_zhs, source.shortname_ko,
        source.shortname_nl, source.shortname_pt, source.shortname_ru, source.shortname_zht
    };
    const std::array<const char*, 12> publishers{
        source.publisher_ja, source.publisher_en, source.publisher_fr, source.publisher_de,
        source.publisher_it, source.publisher_es, source.publisher_zhs, source.publisher_ko,
        source.publisher_nl, source.publisher_pt, source.publisher_ru, source.publisher_zht
    };
    for (std::size_t i = 0; i < longNames.size(); ++i) {
        if (target.longNames[i].empty() && longNames[i] != nullptr)
            target.longNames[i] = DecodeAcpString(longNames[i], 128U);
        if (target.shortNames[i].empty() && shortNames[i] != nullptr)
            target.shortNames[i] = DecodeAcpString(shortNames[i], 64U);
        if (target.publishers[i].empty() && publishers[i] != nullptr)
            target.publishers[i] = DecodeAcpString(publishers[i], 128U);
    }

    // Homebrew on Menu supplies a generic placeholder Product Code for every
    // injected app. Do not mistake that placeholder for app-specific metadata.
    const std::string productCode = CopyFixedString(source.product_code);
    if (target.productCode.empty() && productCode != "WUP-P-HBLD")
        target.productCode = DecodeXmlEntities(productCode);
}

std::string ToFilesystemPath(std::string_view volumePath) {
    if (volumePath.size() > 5U && volumePath.substr(0, 5) == "/vol/") {
        const std::size_t slash = volumePath.find('/', 5U);
        if (slash != std::string_view::npos) {
            std::string result;
            result.reserve(volumePath.size());
            result.append(volumePath.substr(5U, slash - 5U));
            result.push_back(':');
            result.append(volumePath.substr(slash));
            return result;
        }
    }
    return std::string(volumePath);
}

bool ReadFileBounded(const std::string& path, std::size_t maxBytes, std::string& output) {
    output.clear();
    FILE* file = nullptr;
#if defined(_MSC_VER) || defined(_WIN32)
    if (fopen_s(&file, path.c_str(), "rb") != 0) return false;
#else
    file = std::fopen(path.c_str(), "rb");
#endif
    if (file == nullptr) return false;

    bool success = false;
    if (std::fseek(file, 0, SEEK_END) == 0) {
        const long length = std::ftell(file);
        if (length >= 0 && static_cast<unsigned long>(length) <= maxBytes &&
            std::fseek(file, 0, SEEK_SET) == 0) {
            output.resize(static_cast<std::size_t>(length));
            const std::size_t read = output.empty() ? 0U :
                std::fread(output.data(), 1U, output.size(), file);
            success = read == output.size() && std::ferror(file) == 0;
            if (!success) output.clear();
        }
    }
    std::fclose(file);
    return success;
}


#if defined(__WIIU__)
constexpr const char* kHomebrewAppsRoot = "/vol/external01/wiiu/apps";
bool gWuhbUtilsInitialized = false;

std::uint32_t HashHomebrewRelativePath(std::string_view value) noexcept {
    std::uint32_t hash = 0U;
    for (const unsigned char character : value)
        hash = 37U * hash + character;
    return hash;
}

std::string TrimCopy(std::string_view value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1U));
}

bool HasExtension(std::string_view filename, std::string_view extension) noexcept {
    if (filename.size() < extension.size()) return false;
    const std::size_t start = filename.size() - extension.size();
    for (std::size_t i = 0; i < extension.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(filename[start + i]);
        const unsigned char b = static_cast<unsigned char>(extension[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

void ParseHomebrewIni(std::string_view ini, TitleMetadata& output) {
    bool inMenu = false;
    std::size_t position = 0U;
    while (position < ini.size()) {
        const std::size_t newline = ini.find('\n', position);
        const std::size_t end = newline == std::string_view::npos ? ini.size() : newline;
        std::string line = TrimCopy(ini.substr(position, end - position));
        position = newline == std::string_view::npos ? ini.size() : newline + 1U;
        if (line.empty() || line.front() == ';' || line.front() == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            inMenu = line == "[menu]" || line == "[Menu]";
            continue;
        }
        if (!inMenu) continue;
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = TrimCopy(std::string_view(line).substr(0U, equals));
        std::string value = DecodeXmlEntities(TrimCopy(std::string_view(line).substr(equals + 1U)));
        if (value.size() >= 2U &&
            ((value.front() == '"' && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\'')))
            value = value.substr(1U, value.size() - 2U);
        if (key == "longname") output.longName = value;
        else if (key == "shortname") output.shortName = value;
        else if (key == "author") output.publisher = value;
    }
}

bool ReadWuhbFile(const std::string& mountName, const char* relativePath,
                  std::vector<std::uint8_t>& output) {
    const std::string path = mountName + ":/" + relativePath;
    std::uint8_t* buffer = nullptr;
    std::uint32_t size = 0U;
    if (WUHBUtils_ReadWholeFile(path.c_str(), &buffer, &size) != WUHB_UTILS_RESULT_SUCCESS ||
        buffer == nullptr || size == 0U || size > 2U * 1024U * 1024U) {
        std::free(buffer);
        return false;
    }
    output.assign(buffer, buffer + size);
    std::free(buffer);
    return true;
}

bool IsPlausibleIconData(std::string_view path, const std::vector<std::uint8_t>& bytes) noexcept {
    if (bytes.empty()) return false;
    if (HasExtension(path, ".png")) {
        static constexpr std::uint8_t signature[] = {0x89U, 'P', 'N', 'G', 0x0DU, 0x0AU, 0x1AU, 0x0AU};
        return bytes.size() >= sizeof(signature) &&
            std::equal(std::begin(signature), std::end(signature), bytes.begin());
    }
    if (HasExtension(path, ".tga")) {
        if (bytes.size() < 18U || bytes[1] != 0U) return false;
        const std::uint8_t imageType = bytes[2];
        const std::uint16_t width = static_cast<std::uint16_t>(bytes[12] | (bytes[13] << 8U));
        const std::uint16_t height = static_cast<std::uint16_t>(bytes[14] | (bytes[15] << 8U));
        const std::uint8_t depth = bytes[16];
        const bool trueColour = imageType == 2U || imageType == 10U;
        const bool greyscale = imageType == 3U || imageType == 11U;
        if (width == 0U || height == 0U || width > 4096U || height > 4096U) return false;
        if (trueColour && depth != 16U && depth != 24U && depth != 32U) return false;
        if (greyscale && depth != 8U && depth != 16U) return false;
        return trueColour || greyscale;
    }
    return false;
}

bool ReadWuhbMetadata(std::uint64_t titleId, const std::string& bundlePath,
                      TitleMetadata& output, std::string& error) {
    if (!gWuhbUtilsInitialized) {
        const auto status = WUHBUtils_InitLibrary();
        if (status != WUHB_UTILS_RESULT_SUCCESS) {
            error = "WUHBUtils module unavailable; cannot read WUHB metadata from SD.";
            return false;
        }
        gWuhbUtilsInitialized = true;
    }

    char mountBuffer[32]{};
    std::snprintf(mountBuffer, sizeof(mountBuffer), "titles_sorter_%08X",
                  static_cast<std::uint32_t>(titleId));
    const std::string mountName(mountBuffer);
    std::int32_t mountResult = -1;
    if (WUHBUtils_MountBundle(mountName.c_str(), bundlePath.c_str(),
                              BundleSource_FileDescriptor_CafeOS, &mountResult) != WUHB_UTILS_RESULT_SUCCESS ||
        mountResult < 0) {
        error = "Unable to mount the matching WUHB bundle.";
        return false;
    }

    std::vector<std::uint8_t> iniBytes;
    std::vector<std::uint8_t> iconBytes;
    const bool hasIni = ReadWuhbFile(mountName, "meta/meta.ini", iniBytes);

    // Not every WUHB contains iconTex.tga. Prefer a real square icon when
    // present, then try common icon aliases before using the GamePad/DRC artwork.
    static constexpr const char* iconCandidates[] = {
        "meta/iconTex.tga",
        "meta/icon.tga",
        "meta/icon.png",
        "meta/iconTex.png",
        "iconTex.tga",
        "icon.png",
        // Some bundles only carry their standard GamePad/TV artwork.
        "meta/bootDrcTex.tga",
        "meta/bootTvTex.tga"
    };
    const char* selectedIconPath = nullptr;
    for (const char* candidate : iconCandidates) {
        std::vector<std::uint8_t> candidateBytes;
        if (!ReadWuhbFile(mountName, candidate, candidateBytes) ||
            !IsPlausibleIconData(candidate, candidateBytes)) continue;
        iconBytes = std::move(candidateBytes);
        selectedIconPath = candidate;
        break;
    }

    std::int32_t unmountResult = -1;
    (void)WUHBUtils_UnmountBundle(mountName.c_str(), &unmountResult);

    if (hasIni) {
        const std::string_view ini(reinterpret_cast<const char*>(iniBytes.data()), iniBytes.size());
        ParseHomebrewIni(ini, output);
    }
    if (selectedIconPath != nullptr)
        output.iconData = std::make_shared<const std::vector<std::uint8_t>>(std::move(iconBytes));
    return hasIni || selectedIconPath != nullptr;
}
#endif

SystemLanguage ReadSystemLanguage() {
    std::string xml;
    if (ReadFileBounded("/vol/system/config/system.xml", kMaxSystemXmlBytes, xml)) {
        const auto language = ParseSystemLanguageXml(xml);
        if (language.has_value()) return *language;
    }
    return SystemLanguage::English;
}

} // namespace

void WiiUInstalledMetadataProvider::Reset() {
    state_ = MetadataLoadState::Idle;
    language_ = SystemLanguage::English;
    pending_.clear();
    nextIndex_ = 0;
    catalog_.clear();
    warnings_.clear();
    fatalError_.clear();
    homebrewIndexBuilt_ = false;
    homebrewPaths_.clear();
}

bool WiiUInstalledMetadataProvider::Begin(std::string& error) {
    return BeginInternal({}, false, error);
}

bool WiiUInstalledMetadataProvider::Begin(std::span<const std::uint64_t> requestedTitleIds,
                                           std::string& error) {
    return BeginInternal(requestedTitleIds, true, error);
}

bool WiiUInstalledMetadataProvider::BeginInternal(
    std::span<const std::uint64_t> requestedTitleIds,
    bool restrictToRequestedIds,
    std::string& error) {
    Reset();
    error.clear();
    std::unordered_set<std::uint64_t> requested;
    if (restrictToRequestedIds) {
        requested.reserve(requestedTitleIds.size());
        for (const std::uint64_t id : requestedTitleIds) {
            if (id != 0U) requested.insert(id);
        }
    }
    language_ = ReadSystemLanguage();
    if (restrictToRequestedIds) {
        for (const std::uint64_t id : requestedTitleIds)
            ApplySpecialChannelMetadata(id, language_, catalog_);
    }

    const int32_t handle = MCP_Open();
    if (handle < 0) {
        state_ = MetadataLoadState::Failed;
        error = "MCP_Open failed; installed titles cannot be enumerated.";
        fatalError_ = error;
        return false;
    }

    const MCPError countResult = MCP_TitleCount(handle);
    if (countResult < 0) {
        MCP_Close(handle);
        state_ = MetadataLoadState::Failed;
        error = "MCP_TitleCount failed.";
        fatalError_ = error;
        return false;
    }

    const std::uint32_t titleCount = static_cast<std::uint32_t>(countResult);
    if (titleCount > kMaxInstalledTitles) {
        MCP_Close(handle);
        state_ = MetadataLoadState::Failed;
        error = "MCP reported an unexpectedly large installed-title count.";
        fatalError_ = error;
        return false;
    }
    if (titleCount == 0U) {
        MCP_Close(handle);
        state_ = MetadataLoadState::Completed;
        return true;
    }

    const std::size_t listBytes = static_cast<std::size_t>(titleCount) * sizeof(MCPTitleListType);
    if (listBytes > std::numeric_limits<std::uint32_t>::max()) {
        MCP_Close(handle);
        state_ = MetadataLoadState::Failed;
        error = "MCP title list exceeds the supported buffer size.";
        fatalError_ = error;
        return false;
    }

    std::vector<MCPTitleListType> titleList(titleCount);
    std::uint32_t fetchedCount = titleCount;
    // Do not call MCP_TitleList here: Homebrew on Menu hooks that API and
    // appends synthetic 0x0005000F records after the native result. MCP_TitleCount
    // does not necessarily include those records, so a count-sized buffer can be
    // overrun by the hook. Device-type enumeration returns native installed titles
    // without synthetic Home Menu entries; those are resolved separately through ACP.
    const MCPError listResult = MCP_TitleListByDeviceType(
        handle,
        MCP_DEVICE_TYPE_AUTO,
        &fetchedCount,
        titleList.data(),
        static_cast<std::uint32_t>(listBytes));
    MCP_Close(handle);

    if (listResult < 0) {
        state_ = MetadataLoadState::Failed;
        error = "MCP_TitleListByDeviceType failed.";
        fatalError_ = error;
        return false;
    }

    if (fetchedCount > titleList.size()) {
        state_ = MetadataLoadState::Failed;
        error = "MCP_TitleListByDeviceType returned an invalid title count.";
        fatalError_ = error;
        return false;
    }

    pending_.reserve(fetchedCount);
    for (std::size_t i = 0; i < fetchedCount; ++i) {
        const MCPTitleListType& item = titleList[i];
        const bool relevantTitle = restrictToRequestedIds
            ? requested.find(item.titleId) != requested.end()
            : IsSortableTitle(item.titleId);
        if (!relevantTitle) continue;

        const char* pathEnd = std::find(item.path, item.path + sizeof(item.path), '\0');
        if (pathEnd == item.path + sizeof(item.path)) {
            warnings_.emplace_back("Title " + std::to_string(item.titleId) +
                                   ": MCP returned a non-terminated installation path.");
            continue;
        }
        const std::string_view rawPath(item.path, static_cast<std::size_t>(pathEnd - item.path));
        const std::string titlePath = ToFilesystemPath(rawPath);
        if (titlePath.empty()) {
            warnings_.emplace_back("Title " + std::to_string(item.titleId) +
                                   ": MCP returned an empty installation path.");
            continue;
        }

        PendingTitle pending;
        pending.titleId = item.titleId;
        pending.metaPath = titlePath + "/meta/meta.xml";
        pending.iconPath = titlePath + "/meta/iconTex.tga";
        pending_.push_back(std::move(pending));
    }

    state_ = pending_.empty() ? MetadataLoadState::Completed : MetadataLoadState::Loading;
    return true;
}

bool WiiUInstalledMetadataProvider::LoadStep(std::string& error) {
    error.clear();
    if (state_ == MetadataLoadState::Failed) {
        error = fatalError_;
        return false;
    }
    if (state_ != MetadataLoadState::Loading) return true;

    if (nextIndex_ >= pending_.size()) {
        state_ = MetadataLoadState::Completed;
        return true;
    }

    const PendingTitle& title = pending_[nextIndex_++];
    std::string xml;
    if (!ReadFileBounded(title.metaPath, kMaxMetaXmlBytes, xml)) {
        warnings_.emplace_back("Title " + std::to_string(title.titleId) +
                               ": unable to read meta/meta.xml; ACP metadata fallback will be attempted.");
    } else {
        (void)AddTitleMetadata(title.titleId, xml, language_, catalog_, warnings_);
    }

    // Keep an entry even when meta.xml is unavailable: Titles Info also falls
    // back to ACPGetTitleMetaXml, and the icon path remains independently useful.
    auto [found, inserted] = catalog_.try_emplace(title.titleId);
    (void)inserted;
    found->second.iconPath = title.iconPath;

    // Titles Info first reads localized fields from meta.xml, then falls back
    // to ACPGetTitleMetaXml when the installed title name is unavailable.
    if (found->second.longName.empty()) {
        void* storage = AllocateAlignedAcpMetadata(sizeof(ACPMetaXml));
        if (storage != nullptr) {
            auto* acpMetadata = static_cast<ACPMetaXml*>(storage);
            std::memset(acpMetadata, 0, sizeof(ACPMetaXml));
            if (ACPGetTitleMetaXml(title.titleId, acpMetadata) == 0) {
                LocalizedTitleMetadata merged;
                merged.productCode = found->second.productCode;
                merged.longNames[static_cast<std::size_t>(language_)] = found->second.longName;
                merged.shortNames[static_cast<std::size_t>(language_)] = found->second.shortName;
                merged.publishers[static_cast<std::size_t>(language_)] = found->second.publisher;
                CopyAcpLocalizedMetadata(*acpMetadata, merged);
                const TitleMetadata resolved = ResolveTitleMetadata(merged, language_);
                found->second.longName = resolved.longName;
                found->second.shortName = resolved.shortName;
                found->second.publisher = resolved.publisher;
                if (found->second.productCode.empty())
                    found->second.productCode = resolved.productCode;
            }
            FreeAlignedAcpMetadata(storage);
        }
    }

    // Titles Info uses shortname_en as the final metadata fallback before
    // displaying a raw Title ID. Mirror that behaviour for Sort By = Long Name.
    if (found->second.longName.empty() && !found->second.shortName.empty())
        found->second.longName = found->second.shortName;
    if (found->second.longName.empty())
        warnings_.emplace_back("Title " + std::to_string(title.titleId) +
                               ": no longname/shortname metadata available; preview label falls back to Title ID.");

    ApplySpecialChannelMetadata(title.titleId, language_, catalog_);

    if (nextIndex_ >= pending_.size()) state_ = MetadataLoadState::Completed;
    return true;
}

void WiiUInstalledMetadataProvider::BuildHomebrewIndex() {
    if (homebrewIndexBuilt_) return;
    homebrewIndexBuilt_ = true;
#if defined(__WIIU__)
    DIR* root = opendir(kHomebrewAppsRoot);
    if (root == nullptr) {
        warnings_.emplace_back("Unable to enumerate SD:/wiiu/apps for Homebrew on Menu metadata.");
        return;
    }

    const auto addFile = [this](const std::string& fullPath, const std::string& relativePath,
                                const std::string& filename) {
        if (filename.empty() || filename.front() == '.' || filename.front() == '_' ||
            filename == "temp2.wuhb" ||
            (!HasExtension(filename, ".wuhb") && !HasExtension(filename, ".rpx"))) return;
        const std::uint32_t id = HashHomebrewRelativePath(relativePath);
        homebrewPaths_.try_emplace(id, fullPath);
    };

    dirent* entry = nullptr;
    while ((entry = readdir(root)) != nullptr) {
        const std::string name(entry->d_name);
        if (name == "." || name == ".." || name.empty() || name.front() == '.' || name.front() == '_')
            continue;
        const std::string fullPath = std::string(kHomebrewAppsRoot) + "/" + name;
        struct stat info{};
        if (stat(fullPath.c_str(), &info) != 0) continue;
        if (S_ISREG(info.st_mode)) {
            addFile(fullPath, "wiiu/apps/" + name, name);
            continue;
        }
        if (!S_ISDIR(info.st_mode)) continue;

        DIR* child = opendir(fullPath.c_str());
        if (child == nullptr) continue;
        dirent* childEntry = nullptr;
        while ((childEntry = readdir(child)) != nullptr) {
            const std::string childName(childEntry->d_name);
            if (childName == "." || childName == "..") continue;
            const std::string childPath = fullPath + "/" + childName;
            struct stat childInfo{};
            if (stat(childPath.c_str(), &childInfo) != 0 || !S_ISREG(childInfo.st_mode)) continue;
            addFile(childPath, "wiiu/apps/" + name + "/" + childName, childName);
        }
        closedir(child);
    }
    closedir(root);
#endif
}

bool WiiUInstalledMetadataProvider::AddHomebrewTitleMetadata(std::uint64_t titleId, std::string& error) {
    error.clear();
    if ((titleId >> 32U) != kHomebrewHigh) {
        error = "Title ID is not in the Homebrew on Menu 0005000F family.";
        return false;
    }

    BuildHomebrewIndex();
    const auto pathFound = homebrewPaths_.find(static_cast<std::uint32_t>(titleId));
#if defined(__WIIU__)
    if (pathFound != homebrewPaths_.end() && HasExtension(pathFound->second, ".wuhb")) {
        TitleMetadata metadata;
        std::string bundleError;
        if (ReadWuhbMetadata(titleId, pathFound->second, metadata, bundleError)) {
            if (metadata.longName.empty()) {
                std::string filename = pathFound->second.substr(pathFound->second.find_last_of('/') + 1U);
                const std::size_t extension = filename.find_last_of('.');
                if (extension != std::string::npos) filename.resize(extension);
                metadata.longName = filename;
            }
            if (metadata.shortName.empty()) metadata.shortName = metadata.longName;
            catalog_.insert_or_assign(titleId, std::move(metadata));
            return true;
        }
        warnings_.emplace_back("Title " + std::to_string(titleId) + ": " + bundleError);
        // Keep a meaningful label even if the bundle utility module is missing
        // or its metadata file is malformed.
        std::string filename = pathFound->second.substr(pathFound->second.find_last_of('/') + 1U);
        const std::size_t extension = filename.find_last_of('.');
        if (extension != std::string::npos) filename.resize(extension);
        TitleMetadata fallback;
        fallback.longName = filename;
        fallback.shortName = filename;
        catalog_.insert_or_assign(titleId, std::move(fallback));
        return true;
    } else if (pathFound != homebrewPaths_.end() && HasExtension(pathFound->second, ".rpx")) {
        // Standalone RPX homebrew may ship a neighbouring meta.xml and iconTex.tga.
        const std::string& executable = pathFound->second;
        const std::size_t slash = executable.find_last_of('/');
        const std::string directory = slash == std::string::npos ? std::string{} : executable.substr(0U, slash);
        std::string xml;
        TitleMetadata metadata;
        if (ReadFileBounded(directory + "/meta.xml", kMaxMetaXmlBytes, xml))
            (void)AddTitleMetadata(titleId, xml, language_, catalog_, warnings_);
        if (ReadFileBounded(directory + "/meta/meta.xml", kMaxMetaXmlBytes, xml))
            (void)AddTitleMetadata(titleId, xml, language_, catalog_, warnings_);
        auto found = catalog_.find(titleId);
        if (found != catalog_.end()) metadata = found->second;
        const std::string iconTga = directory + "/meta/iconTex.tga";
        const std::string iconPng = directory + "/icon.png";
        struct stat iconInfo{};
        if (stat(iconTga.c_str(), &iconInfo) == 0 && S_ISREG(iconInfo.st_mode))
            metadata.iconPath = iconTga;
        else if (stat(iconPng.c_str(), &iconInfo) == 0 && S_ISREG(iconInfo.st_mode))
            metadata.iconPath = iconPng;
        if (metadata.longName.empty()) {
            std::string filename = executable.substr(slash == std::string::npos ? 0U : slash + 1U);
            const std::size_t extension = filename.find_last_of('.');
            if (extension != std::string::npos) filename.resize(extension);
            metadata.longName = filename;
        }
        if (metadata.shortName.empty()) metadata.shortName = metadata.longName;
        catalog_.insert_or_assign(titleId, std::move(metadata));
        return true;
    }
#endif

    // ACP is still useful as a fallback on environments that expose this
    // metadata directly, but the Homebrew on Menu plugin hooks the Wii U Menu
    // process only. Do not rely on ACP alone for homebrew launched as a title.
    void* storage = AllocateAlignedAcpMetadata(sizeof(ACPMetaXml));
    if (storage == nullptr) {
        error = "Unable to allocate aligned ACP metadata buffer.";
        warnings_.emplace_back("Title " + std::to_string(titleId) + ": " + error);
        return false;
    }

    auto* metadata = static_cast<ACPMetaXml*>(storage);
    std::memset(metadata, 0, sizeof(ACPMetaXml));
    const int32_t result = ACPGetTitleMetaXml(titleId, metadata);
    if (result != 0) {
        FreeAlignedAcpMetadata(storage);
        error = pathFound == homebrewPaths_.end()
            ? "No matching SD executable was found and ACP metadata was unavailable."
            : "ACP metadata fallback failed for synthetic Homebrew title.";
        warnings_.emplace_back("Title " + std::to_string(titleId) + ": " + error);
        return false;
    }

    LocalizedTitleMetadata localized;
    CopyAcpLocalizedMetadata(*metadata, localized);
    FreeAlignedAcpMetadata(storage);
    TitleMetadata resolved = ResolveTitleMetadata(localized, language_);
    if (resolved.longName.empty()) {
        error = "ACP metadata contains no usable name or publisher fields.";
        warnings_.emplace_back("Title " + std::to_string(titleId) + ": " + error);
        return false;
    }
    // Retain the plugin's virtual path as a fallback for environments that
    // redirect it in this process. Direct WUHB extraction above is preferred.
    char iconPath[96]{};
    std::snprintf(iconPath, sizeof(iconPath),
                  "storage_mlc01:/sys/title/0005000F/%08X/meta/iconTex.tga",
                  static_cast<std::uint32_t>(titleId & 0xFFFFFFFFULL));
    resolved.iconPath = iconPath;
    catalog_.insert_or_assign(titleId, resolved);
    return true;
}

bool WiiUInstalledMetadataProvider::IsLoading() const noexcept {
    return state_ == MetadataLoadState::Loading;
}

bool WiiUInstalledMetadataProvider::IsComplete() const noexcept {
    return state_ == MetadataLoadState::Completed;
}

MetadataLoadProgress WiiUInstalledMetadataProvider::Progress() const noexcept {
    return {state_, nextIndex_, pending_.size(), catalog_.size(), warnings_.size()};
}

SystemLanguage WiiUInstalledMetadataProvider::Language() const noexcept { return language_; }
const MetadataCatalog& WiiUInstalledMetadataProvider::Catalog() const noexcept { return catalog_; }
const std::vector<std::string>& WiiUInstalledMetadataProvider::Warnings() const noexcept { return warnings_; }

} // namespace titles_sorter

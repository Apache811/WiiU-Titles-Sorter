#include "platform/wiiu/WiiUHomeMenuSavePlatform.hpp"

#include <coreinit/dynload.h>
#include <coreinit/mcp.h>
#include <mocha/mocha.h>
#include <nn/act.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

namespace titles_sorter {
namespace {

using SystemApplicationTitleIdFn = std::uint64_t (*)(std::int32_t);

class WiiUReadOnlyFileSystem final : public IReadOnlySaveFileSystem {
public:
    SaveFileStatus GetFileSize(const std::string& path, std::size_t& size) override {
        size = 0U;
        struct stat fileInfo {};
        if (stat(path.c_str(), &fileInfo) != 0) {
            return (errno == ENOENT || errno == ENOTDIR)
                ? SaveFileStatus::NotFound : SaveFileStatus::IoError;
        }
        if (!S_ISREG(fileInfo.st_mode) || fileInfo.st_size < 0) {
            return SaveFileStatus::IoError;
        }
        const auto unsignedSize = static_cast<std::uintmax_t>(fileInfo.st_size);
        if (unsignedSize > static_cast<std::uintmax_t>(SIZE_MAX)) {
            return SaveFileStatus::TooLarge;
        }
        size = static_cast<std::size_t>(unsignedSize);
        if (size > kExpectedHomeMenuSaveSize) return SaveFileStatus::TooLarge;
        if (size < kExpectedHomeMenuSaveSize) return SaveFileStatus::TooSmall;
        return SaveFileStatus::Success;
    }

    SaveFileStatus ReadExact(
        const std::string& path,
        std::uint8_t* destination,
        std::size_t size) override {
        if (destination == nullptr || size != kExpectedHomeMenuSaveSize) {
            return SaveFileStatus::IoError;
        }
        std::FILE* file = nullptr;
#if defined(_MSC_VER)
        if (fopen_s(&file, path.c_str(), "rb") != 0) return SaveFileStatus::IoError;
#else
        file = std::fopen(path.c_str(), "rb");
#endif
        if (file == nullptr) return SaveFileStatus::IoError;

        const std::size_t readCount = std::fread(destination, 1U, size, file);
        const bool readFailed = readCount != size || std::ferror(file) != 0;
        const int trailingByte = readFailed ? EOF : std::fgetc(file);
        const bool hasUnexpectedTrailingData = trailingByte != EOF;
        const bool streamError = std::ferror(file) != 0;
        const int closeResult = std::fclose(file);

        if (readFailed || streamError || closeResult != 0) return SaveFileStatus::IoError;
        if (hasUnexpectedTrailingData) return SaveFileStatus::TooLarge;
        return SaveFileStatus::Success;
    }
};

bool GetSystemMenuTitleId(std::uint64_t& titleId, std::string& error) {
    OSDynLoad_Module module{};
    if (OSDynLoad_Acquire("sysapp.rpl", &module) != OS_DYNLOAD_OK) {
        error = "Unable to load sysapp.rpl.";
        return false;
    }

    SystemApplicationTitleIdFn getTitleId = nullptr;
    const OSDynLoad_Error exportResult = OSDynLoad_FindExport(
        module, OS_DYNLOAD_EXPORT_FUNC, "_SYSGetSystemApplicationTitleId",
        reinterpret_cast<void**>(&getTitleId));
    if (exportResult != OS_DYNLOAD_OK || getTitleId == nullptr) {
        error = "Unable to resolve _SYSGetSystemApplicationTitleId.";
        return false;
    }

    titleId = getTitleId(0);
    if ((titleId & 0xFFFFFFFF00000000ULL) != 0x0005001000000000ULL ||
        static_cast<std::uint32_t>(titleId) == 0U) {
        error = "The system application identifier is not a valid Wii U Menu title.";
        return false;
    }
    return true;
}

bool GetActiveAccountPersistentId(std::uint32_t& persistentId, std::string& error) {
    persistentId = 0U;
    nn::act::Initialize();
    const std::uint8_t slot = nn::act::GetSlotNo();
    if (slot == 0xFFU) {
        nn::act::Finalize();
        error = "ACT did not return a valid active account slot.";
        return false;
    }
    persistentId = nn::act::GetPersistentIdEx(slot);
    nn::act::Finalize();
    if (persistentId == 0U) {
        error = "ACT returned an invalid active account persistent ID.";
        return false;
    }
    return true;
}

bool GetConsoleSerialId(std::string& serialId, std::string& error) {
    serialId.clear();
    const int handle = MCP_Open();
    if (handle < 0) {
        error = "MCP_Open failed while retrieving console identity.";
        return false;
    }

    MCPSysProdSettings settings __attribute__((aligned(0x40)));
    std::memset(&settings, 0, sizeof(settings));
    const int result = MCP_GetSysProdSettings(handle, &settings);
    MCP_Close(handle);
    if (result != 0) {
        error = "MCP_GetSysProdSettings failed while retrieving console identity.";
        return false;
    }

    char codeId[sizeof(settings.code_id) + 1U]{};
    char unitSerialId[sizeof(settings.serial_id) + 1U]{};
    std::memcpy(codeId, settings.code_id, sizeof(settings.code_id));
    std::memcpy(unitSerialId, settings.serial_id, sizeof(settings.serial_id));
    serialId.reserve(sizeof(settings.code_id) + sizeof(settings.serial_id));
    serialId.append(codeId, strnlen(codeId, sizeof(settings.code_id)));
    serialId.append(unitSerialId, strnlen(unitSerialId, sizeof(settings.serial_id)));
    if (serialId.empty()) {
        error = "MCP returned an empty console serial identity.";
        return false;
    }
    return true;
}


} // namespace

bool WiiUHomeMenuSavePlatform::ReadActiveSave(
    WiiUHomeMenuSaveSnapshot& snapshot,
    std::string& error) {
    error.clear();

    WiiUHomeMenuSaveIdentity identity{};
    if (!GetSystemMenuTitleId(identity.systemMenuTitleId, error)) return false;
    if (!GetActiveAccountPersistentId(identity.userPersistentId, error)) return false;
    if (!GetConsoleSerialId(identity.consoleSerialId, error)) return false;

    // Mocha and the MLC volume are initialized once by main() and remain
    // mounted for the full application session, matching Titles Info.
    // The plugin's redirected file lives on the SD volume; MLC is only the
    // fallback path. Do not mount or access USB storage here.
    WiiUReadOnlyFileSystem fileSystem;
    return ReadActiveHomeMenuSave(identity, fileSystem, snapshot, error);
}

} // namespace titles_sorter

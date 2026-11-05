#include "core/HomeMenuSaveBackupManager.hpp"

#include "core/TitlesSorterCore.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <dirent.h>
#if defined(_WIN32)
#include <direct.h>
#endif
#include <string_view>
#include <sys/stat.h>
#include <vector>

namespace titles_sorter {
namespace {

constexpr std::size_t kSaveSize = kExpectedSaveSize;

bool IsRegularFile(const std::string& path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool EnsureDirectory(const std::string& path, std::string& error) {
    if (path.empty()) { error = "Backup directory path is empty."; return false; }
    struct stat info {};
    if (stat(path.c_str(), &info) == 0) {
        if (S_ISDIR(info.st_mode)) return true;
        error = "Backup path exists but is not a directory.";
        return false;
    }
    if (errno != ENOENT) {
        error = "Unable to inspect backup directory.";
        return false;
    }
#if defined(_WIN32)
    const int createResult = _mkdir(path.c_str());
#else
    const int createResult = mkdir(path.c_str(), 0777);
#endif
    if (createResult != 0) {
        error = "Unable to create backup directory.";
        return false;
    }
    return true;
}

bool ReadSave(const std::string& path, std::vector<std::uint8_t>& bytes,
              std::string& error) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) { error = "Unable to open save file."; return false; }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file); error = "Unable to inspect save file."; return false;
    }
    const long length = std::ftell(file);
    if (length != static_cast<long>(kSaveSize) || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file); error = "Save file has an unsupported size."; return false;
    }
    std::vector<std::uint8_t> candidate(kSaveSize);
    const std::size_t count = std::fread(candidate.data(), 1U, candidate.size(), file);
    const bool failed = count != candidate.size() || std::ferror(file) != 0;
    const int closeResult = std::fclose(file);
    if (failed || closeResult != 0) {
        error = "Unable to read the complete save file."; return false;
    }
    const ParseResult parsed = ParseSave(candidate);
    if (!parsed) { error = "Save file failed structural validation."; return false; }
    bytes = std::move(candidate);
    return true;
}

bool WriteExact(const std::string& path, const std::vector<std::uint8_t>& bytes,
                std::string& error) {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) { error = "Unable to create staged save file."; return false; }
    const std::size_t count = std::fwrite(bytes.data(), 1U, bytes.size(), file);
    const bool flushFailed = std::fflush(file) != 0;
    const int closeResult = std::fclose(file);
    if (count != bytes.size() || flushFailed || closeResult != 0) {
        std::remove(path.c_str());
        error = "Unable to write and flush the complete staged save.";
        return false;
    }
    return true;
}

bool IsBackupName(std::string_view name, bool& automatic) {
    constexpr std::string_view autoPrefix = "autobackup_";
    constexpr std::string_view manualPrefix = "backup_";
    automatic = name.substr(0U, autoPrefix.size()) == autoPrefix;
    if (!automatic && name.substr(0U, manualPrefix.size()) != manualPrefix) return false;
    if (name.size() < 30U || name.substr(name.size() - 4U) != ".dat") return false;
    // YYYY-MM-DD_HH-MM-SS: fixed-width timestamp, lexicographically sortable.
    const std::size_t offset = automatic ? autoPrefix.size() : manualPrefix.size();
    if (offset + 23U != name.size()) return false;
    constexpr char separators[] = {'-', '-', '_', '-', '-'};
    for (std::size_t i = offset; i < name.size() - 4U; ++i) {
        const char c = name[i];
        const std::size_t relative = i - offset;
        if (relative == 4U || relative == 7U || relative == 10U ||
            relative == 13U || relative == 16U) {
            const std::size_t separatorIndex = relative == 4U ? 0U :
                relative == 7U ? 1U : relative == 10U ? 2U :
                relative == 13U ? 3U : 4U;
            if (c != separators[separatorIndex]) return false;
        } else if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

std::string_view BackupTimestamp(const std::string& fileName) noexcept {
    constexpr std::string_view automaticPrefix = "autobackup_";
    constexpr std::string_view manualPrefix = "backup_";
    const std::string_view name(fileName);
    const std::size_t offset = name.substr(0U, automaticPrefix.size()) == automaticPrefix
        ? automaticPrefix.size() : manualPrefix.size();
    return name.substr(offset, 19U);
}

std::string Join(const std::string& directory, const std::string& file) {
    return directory + (directory.back() == '/' ? "" : "/") + file;
}

bool Enumerate(const std::string& directory, std::vector<SaveBackupEntry>& entries,
               std::string& error) {
    DIR* handle = opendir(directory.c_str());
    if (handle == nullptr) {
        if (errno == ENOENT) { entries.clear(); return true; }
        error = "Unable to enumerate backup directory.";
        return false;
    }
    std::vector<SaveBackupEntry> result;
    while (dirent* item = readdir(handle)) {
        const std::string name(item->d_name);
        bool automatic = false;
        if (IsBackupName(name, automatic) && IsRegularFile(Join(directory, name)))
            result.push_back({name, automatic});
    }
    const int closeResult = closedir(handle);
    if (closeResult != 0) { error = "Unable to close backup directory."; return false; }
    std::sort(result.begin(), result.end(),
        [](const SaveBackupEntry& a, const SaveBackupEntry& b) {
            const std::string_view timestampA = BackupTimestamp(a.fileName);
            const std::string_view timestampB = BackupTimestamp(b.fileName);
            if (timestampA != timestampB) return timestampA > timestampB;
            return a.fileName > b.fileName;
        });
    entries = std::move(result);
    return true;
}

std::string TimestampName(SaveBackupKind kind, const std::string& backupDirectory) {
    const std::time_t now = std::time(nullptr);
    const char* prefix = kind == SaveBackupKind::Automatic ? "autobackup_" : "backup_";
    // Backup creation can happen more than once in a second (for example,
    // Apply followed immediately by Restore). Keep the established filename
    // format while advancing to the first unused second.
    for (std::time_t candidate = now; candidate <= now + 3600; ++candidate) {
        std::tm local{};
#if defined(_WIN32)
        if (localtime_s(&local, &candidate) != 0) return {};
#else
        if (localtime_r(&candidate, &local) == nullptr) return {};
#endif
        char buffer[48]{};
        const int length = std::snprintf(buffer, sizeof(buffer),
            "%s%04d-%02d-%02d_%02d-%02d-%02d.dat", prefix,
            local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
            local.tm_hour, local.tm_min, local.tm_sec);
        if (length <= 0 || static_cast<std::size_t>(length) >= sizeof(buffer)) return {};
        const std::string candidateName(buffer);
        const std::string candidatePath = Join(backupDirectory, candidateName);
        struct stat info {};
        if (stat(candidatePath.c_str(), &info) != 0 && errno == ENOENT)
            return candidateName;
    }
    return {};
}

} // namespace

bool HomeMenuSaveBackupManager::CreateBackup(
    const std::string& activeSavePath, const std::string& backupDirectory,
    SaveBackupKind kind, std::size_t maxAutomaticBackups,
    std::string& createdPath, std::string& error) const {
    error.clear();
    createdPath.clear();
    if (kind == SaveBackupKind::Automatic && maxAutomaticBackups == 0U) {
        error = "The automatic backup retention limit must be at least one.";
        return false;
    }
    std::vector<std::uint8_t> source;
    if (!ReadSave(activeSavePath, source, error)) return false;
    if (!EnsureDirectory(backupDirectory, error)) return false;

    const std::string fileName = TimestampName(kind, backupDirectory);
    if (fileName.empty()) { error = "Unable to generate backup timestamp."; return false; }
    const std::string destination = Join(backupDirectory, fileName);
    if (IsRegularFile(destination)) {
        error = "A backup with this timestamp already exists; refusing to overwrite it.";
        return false;
    }
    const std::string temporary = destination + ".tmp";
    if (!WriteExact(temporary, source, error)) return false;
    std::vector<std::uint8_t> verification;
    if (!ReadSave(temporary, verification, error) || verification != source) {
        std::remove(temporary.c_str());
        if (error.empty()) error = "Backup verification failed.";
        return false;
    }
    std::vector<std::uint8_t>().swap(verification);
    std::vector<std::uint8_t>().swap(source);
    if (std::rename(temporary.c_str(), destination.c_str()) != 0) {
        std::remove(temporary.c_str());
        error = "Unable to commit completed backup.";
        return false;
    }
    createdPath = destination;

    if (kind == SaveBackupKind::Automatic) {
        std::vector<SaveBackupEntry> entries;
        if (!Enumerate(backupDirectory, entries, error)) return false;
        std::size_t automaticCount = 0U;
        for (const auto& entry : entries) if (entry.automatic) ++automaticCount;
        // Entries are newest-first; retain the newest N and delete only older autobackups.
        for (std::size_t i = entries.size(); i > 0U && automaticCount > maxAutomaticBackups; --i) {
            const auto& entry = entries[i - 1U];
            if (!entry.automatic) continue;
            if (std::remove(Join(backupDirectory, entry.fileName).c_str()) != 0) {
                error = "Backup was created, but an expired automatic backup could not be removed.";
                return false;
            }
            --automaticCount;
        }
    }
    return true;
}

bool HomeMenuSaveBackupManager::ListBackups(
    const std::string& backupDirectory, std::vector<SaveBackupEntry>& entries,
    std::string& error) const {
    error.clear();
    return Enumerate(backupDirectory, entries, error);
}

bool HomeMenuSaveBackupManager::RecoverInterruptedRestore(
    const std::string& activeSavePath, std::string& error) const {
    error.clear();
    if (activeSavePath.empty()) {
        error = "The active Home Menu save path is empty.";
        return false;
    }
    const std::string previous = activeSavePath + ".restore.previous";
    const std::string temporary = activeSavePath + ".restore.tmp";
    const std::string failed = activeSavePath + ".restore.failed";
    struct stat previousInfo {};
    if (stat(previous.c_str(), &previousInfo) != 0) {
        if (errno != ENOENT) {
            error = "Unable to inspect the interrupted Restore recovery file.";
            return false;
        }
        if (std::remove(temporary.c_str()) != 0 && errno != ENOENT) {
            error = "Unable to remove an abandoned Restore staging file.";
            return false;
        }
        return true;
    }

    std::vector<std::uint8_t> original;
    if (!ReadSave(previous, original, error)) {
        error = "A Restore recovery file exists but is not a valid save. " + error;
        return false;
    }
    struct stat failedInfo {};
    if (stat(failed.c_str(), &failedInfo) == 0) {
        error = "A previous Restore failure artifact exists; manual recovery is required: " + failed;
        return false;
    }
    if (errno != ENOENT) {
        error = "Unable to inspect the Restore failure artifact path.";
        return false;
    }
    struct stat activeInfo {};
    const bool activeExists = stat(activeSavePath.c_str(), &activeInfo) == 0;
    if (!activeExists && errno != ENOENT) {
        error = "Unable to inspect the active save during Restore recovery.";
        return false;
    }
    if (activeExists && std::rename(activeSavePath.c_str(), failed.c_str()) != 0) {
        error = "Unable to preserve the interrupted Restore candidate.";
        return false;
    }
    if (std::rename(previous.c_str(), activeSavePath.c_str()) != 0) {
        if (activeExists) (void)std::rename(failed.c_str(), activeSavePath.c_str());
        error = "Unable to restore the original save after an interrupted Restore.";
        return false;
    }
    std::vector<std::uint8_t> restored;
    if (!ReadSave(activeSavePath, restored, error) || restored != original) {
        error = "Recovered Restore save failed verification. Preserve all .restore files and do not launch the Wii U Menu.";
        return false;
    }
    if (activeExists) (void)std::remove(failed.c_str());
    (void)std::remove(temporary.c_str());
    return true;
}

bool HomeMenuSaveBackupManager::RestoreBackup(
    const std::string& backupPath, const std::string& activeSavePath,
    const std::string& backupDirectory, bool createAutomaticBackup,
    std::size_t maxAutomaticBackups,
    std::string& automaticBackupPath, std::string& error) const {
    error.clear();
    automaticBackupPath.clear();
    if (backupPath == activeSavePath) {
        error = "The selected backup path is the active save path.";
        return false;
    }
    const std::string temporary = activeSavePath + ".restore.tmp";
    const std::string previous = activeSavePath + ".restore.previous";
    struct stat recoveryInfo {};
    if (stat(previous.c_str(), &recoveryInfo) == 0) {
        error = "A previous restore recovery file already exists; resolve it before restoring again.";
        return false;
    }
    if (errno != ENOENT) {
        error = "Unable to inspect the previous restore recovery file.";
        return false;
    }

    // Stage the selected backup before retention can prune its source file.
    // Once the staged file has been read back and compared, release both
    // in-memory copies before creating the optional safety autobackup.
    std::vector<std::uint8_t> backup;
    if (!ReadSave(backupPath, backup, error)) return false;
    std::remove(temporary.c_str());
    if (!WriteExact(temporary, backup, error)) return false;
    std::vector<std::uint8_t> staged;
    if (!ReadSave(temporary, staged, error) || staged != backup) {
        std::remove(temporary.c_str());
        if (error.empty()) error = "Staged restore verification failed.";
        return false;
    }
    std::vector<std::uint8_t>().swap(staged);
    std::vector<std::uint8_t>().swap(backup);

    if (createAutomaticBackup &&
        !CreateBackup(activeSavePath, backupDirectory,
                      SaveBackupKind::Automatic, maxAutomaticBackups,
                      automaticBackupPath, error)) {
        // The selected backup is already staged, so retention may safely
        // remove its original file. Do not continue unless the active save
        // has first been preserved successfully.
        std::remove(temporary.c_str());
        return false;
    }

    // Re-read the already-verified staged file only after autobackup has
    // completed. This keeps peak memory to two save buffers rather than
    // retaining the selected backup while CreateBackup allocates its own.
    if (!ReadSave(temporary, backup, error)) {
        std::remove(temporary.c_str());
        return false;
    }

    // Preserve the current save until the staged replacement is ready and validated.
    if (std::rename(activeSavePath.c_str(), previous.c_str()) != 0) {
        std::remove(temporary.c_str());
        error = "Unable to preserve the current Home Menu save; restore was cancelled.";
        return false;
    }
    if (std::rename(temporary.c_str(), activeSavePath.c_str()) != 0) {
        (void)std::rename(previous.c_str(), activeSavePath.c_str());
        std::remove(temporary.c_str());
        error = "Unable to install restored save; the previous save was preserved.";
        return false;
    }

    std::vector<std::uint8_t> installed;
    if (!ReadSave(activeSavePath, installed, error) || installed != backup) {
        std::remove(activeSavePath.c_str());
        if (std::rename(previous.c_str(), activeSavePath.c_str()) != 0)
            error = "Restore verification failed and automatic rollback also failed. The previous save remains at the recovery file.";
        else if (error.empty())
            error = "Restore verification failed; the previous save was restored.";
        return false;
    }
    std::remove(previous.c_str());
    return true;
}

} // namespace titles_sorter

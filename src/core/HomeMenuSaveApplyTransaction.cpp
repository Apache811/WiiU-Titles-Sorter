#include "core/HomeMenuSaveApplyTransaction.hpp"

#include "core/HomeMenuSaveBackupManager.hpp"
#include "core/HomeMenuSavePatcher.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <utility>
#include <vector>

namespace titles_sorter {
namespace {

bool Exists(const std::string& path, bool& exists, std::string& error) {
    struct stat info {};
    if (stat(path.c_str(), &info) == 0) {
        exists = true;
        return true;
    }
    if (errno == ENOENT || errno == ENOTDIR) {
        exists = false;
        return true;
    }
    error = "Unable to inspect transaction path: " + path;
    return false;
}

bool ReadExactSave(const std::string& path, std::vector<std::uint8_t>& bytes,
                   std::string& error) {
    struct stat info {};
    if (stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode) ||
        info.st_size < 0 || static_cast<unsigned long long>(info.st_size) !=
            static_cast<unsigned long long>(kExpectedSaveSize)) {
        error = "The active save is missing, is not a regular file, or has an unsupported size.";
        return false;
    }
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        error = "Unable to open the active Home Menu save for reading.";
        return false;
    }
    std::vector<std::uint8_t> candidate(kExpectedSaveSize);
    const std::size_t read = std::fread(candidate.data(), 1U, candidate.size(), file);
    const bool failed = read != candidate.size() || std::ferror(file) != 0;
    const int trailing = failed ? EOF : std::fgetc(file);
    const bool closeFailed = std::fclose(file) != 0;
    if (failed || trailing != EOF || closeFailed) {
        error = "Unable to read the complete active Home Menu save.";
        return false;
    }
    bytes = std::move(candidate);
    return true;
}

bool WriteExactSave(const std::string& path, const std::vector<std::uint8_t>& bytes,
                    std::string& error) {
    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        error = "Unable to create the staged Home Menu save.";
        return false;
    }
    const std::size_t written = std::fwrite(bytes.data(), 1U, bytes.size(), file);
    const bool flushFailed = std::fflush(file) != 0;
    const bool closeFailed = std::fclose(file) != 0;
    if (written != bytes.size() || flushFailed || closeFailed) {
        std::remove(path.c_str());
        error = "Unable to write the complete staged Home Menu save.";
        return false;
    }
    return true;
}

bool ValidateSaveBytes(const std::vector<std::uint8_t>& bytes, std::string& error) {
    if (bytes.size() != kExpectedSaveSize) {
        error = "The candidate save has an unsupported size.";
        return false;
    }
    const ParseResult parsed = ParseSave(bytes);
    if (!parsed) {
        error = "The candidate save failed structural validation.";
        if (!parsed.errors.empty()) error += " " + parsed.errors.front();
        return false;
    }
    return true;
}

} // namespace

bool HomeMenuSaveApplyTransaction::RecoverInterruptedApply(
    const std::string& activeSavePath, std::string& error) const {
    error.clear();
    if (activeSavePath.empty()) {
        error = "The active Home Menu save path is empty.";
        return false;
    }
    const std::string stagedPath = activeSavePath + ".apply.tmp";
    const std::string previousPath = activeSavePath + ".apply.previous";
    bool stagedExists = false;
    bool previousExists = false;
    bool activeExists = false;
    if (!Exists(stagedPath, stagedExists, error) ||
        !Exists(previousPath, previousExists, error) ||
        !Exists(activeSavePath, activeExists, error)) return false;

    if (!previousExists) {
        // A staged file without a previous-save rename was never committed.
        if (stagedExists && std::remove(stagedPath.c_str()) != 0) {
            error = "An abandoned Apply staging file could not be removed.";
            return false;
        }
        return true;
    }

    std::vector<std::uint8_t> previousBytes;
    if (!ReadExactSave(previousPath, previousBytes, error) ||
        !ValidateSaveBytes(previousBytes, error)) {
        error = "An Apply recovery file exists but is not a valid Home Menu save. " + error;
        return false;
    }

    // A previous file means the transaction did not reach its final cleanup.
    // Prefer restoring the original, even if the candidate currently exists: a
    // crash may have occurred before post-install verification completed.
    const std::string failedPath = activeSavePath + ".apply.failed";
    bool failedExists = false;
    if (!Exists(failedPath, failedExists, error)) return false;
    if (failedExists) {
        error = "An earlier failed Apply artifact already exists; manual recovery is required: " + failedPath;
        return false;
    }
    if (activeExists && std::rename(activeSavePath.c_str(), failedPath.c_str()) != 0) {
        error = "Unable to preserve the interrupted Apply candidate for recovery.";
        return false;
    }
    if (std::rename(previousPath.c_str(), activeSavePath.c_str()) != 0) {
        if (activeExists) (void)std::rename(failedPath.c_str(), activeSavePath.c_str());
        error = "Unable to restore the original save from the interrupted Apply recovery file.";
        return false;
    }

    std::vector<std::uint8_t> restoredBytes;
    if (!ReadExactSave(activeSavePath, restoredBytes, error) ||
        restoredBytes != previousBytes || !ValidateSaveBytes(restoredBytes, error)) {
        error = "The recovered original save failed verification. Keep all .apply recovery files and do not launch the Wii U Menu.";
        return false;
    }
    if (activeExists) (void)std::remove(failedPath.c_str());
    if (stagedExists) (void)std::remove(stagedPath.c_str());
    return true;
}

ApplyTransactionResult HomeMenuSaveApplyTransaction::Apply(
    const SaveModel& model, const PlanResult& plan,
    const std::string& activeSavePath, const std::string& backupDirectory,
    const ApplyTransactionOptions& options) const {
    ApplyTransactionResult result;
    if (activeSavePath.empty()) {
        result.error = "The active Home Menu save path is empty.";
        return result;
    }
    if (options.createAutomaticBackup && options.maxAutomaticBackups == 0U) {
        result.error = "The automatic backup retention limit must be at least one.";
        return result;
    }

    std::vector<std::uint8_t> activeBytes;
    if (!ReadExactSave(activeSavePath, activeBytes, result.error)) return result;
    if (activeBytes != model.originalBytes) {
        result.error = "The active save changed after preview generation. Reload the save and generate a new preview before applying.";
        return result;
    }
    if (!ValidateSaveBytes(activeBytes, result.error)) return result;
    // The active bytes have been compared and validated; release this full-save
    // buffer before building the candidate to reduce peak transaction memory.
    std::vector<std::uint8_t>().swap(activeBytes);

    HomeMenuSavePatcher patcher;
    std::vector<std::uint8_t> candidate;
    if (!patcher.BuildPatchedSave(model, plan, candidate, result.error)) return result;
    if (!ValidateSaveBytes(candidate, result.error)) return result;

    const std::string stagedPath = activeSavePath + ".apply.tmp";
    const std::string previousPath = activeSavePath + ".apply.previous";
    bool stagedExists = false;
    bool previousExists = false;
    if (!Exists(stagedPath, stagedExists, result.error) ||
        !Exists(previousPath, previousExists, result.error)) return result;
    if (stagedExists || previousExists) {
        result.error = "An unfinished Apply transaction already exists. Resolve its recovery files before applying again.";
        return result;
    }

    if (options.createAutomaticBackup) {
        HomeMenuSaveBackupManager backupManager;
        const bool backupSucceeded = backupManager.CreateBackup(
            activeSavePath, backupDirectory, SaveBackupKind::Automatic,
            options.maxAutomaticBackups, result.backupPath, result.error);
        result.backupCreated = !result.backupPath.empty();
        if (!backupSucceeded) return result;
    }

    if (!WriteExactSave(stagedPath, candidate, result.error)) return result;
    std::vector<std::uint8_t> stagedBytes;
    if (!ReadExactSave(stagedPath, stagedBytes, result.error) ||
        stagedBytes != candidate || !ValidateSaveBytes(stagedBytes, result.error)) {
        std::remove(stagedPath.c_str());
        if (result.error.empty()) result.error = "The staged Apply file did not match the validated candidate.";
        return result;
    }
    // Candidate and on-disk staged file are now verified; the staged readback
    // buffer is no longer needed for the rename/commit phase.
    std::vector<std::uint8_t>().swap(stagedBytes);

    // Keep the original file under a recovery name until the replacement has
    // been reopened and verified. Both paths are siblings, avoiding cross-volume rename.
    if (std::rename(activeSavePath.c_str(), previousPath.c_str()) != 0) {
        std::remove(stagedPath.c_str());
        result.error = "Unable to preserve the original Home Menu save. Apply was cancelled.";
        return result;
    }
    if (std::rename(stagedPath.c_str(), activeSavePath.c_str()) != 0) {
        if (std::rename(previousPath.c_str(), activeSavePath.c_str()) != 0)
            result.error = "Unable to install the candidate and automatic rollback failed. The original save remains at: " + previousPath;
        else
            result.error = "Unable to install the candidate. The original save was restored.";
        std::remove(stagedPath.c_str());
        return result;
    }

    std::vector<std::uint8_t> installedBytes;
    if (!ReadExactSave(activeSavePath, installedBytes, result.error) ||
        installedBytes != candidate || !ValidateSaveBytes(installedBytes, result.error)) {
        std::remove(activeSavePath.c_str());
        if (std::rename(previousPath.c_str(), activeSavePath.c_str()) != 0)
            result.error = "Post-write verification failed and rollback failed. The original save remains at: " + previousPath;
        else if (result.error.empty())
            result.error = "Post-write verification failed. The original save was restored.";
        return result;
    }

    if (std::remove(previousPath.c_str()) != 0) {
        result.error = "Apply succeeded, but the previous-save recovery file could not be removed: " + previousPath;
        result.applied = true;
        return result;
    }
    result.applied = true;
    return result;
}

} // namespace titles_sorter

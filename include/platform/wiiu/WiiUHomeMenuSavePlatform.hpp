#pragma once

#include "core/HomeMenuSaveBackupManager.hpp"
#include "core/WiiUHomeMenuSaveReader.hpp"
#include "core/HomeMenuSaveApplyTransaction.hpp"

#include <string>
#include <vector>

namespace titles_sorter {

// Wii U save acquisition and write operations. Read/recovery runs before the
// active save is parsed. Mutating operations are explicit and use the same
// authoritative path selected by the reader (plugin redirect or MLC).
class WiiUHomeMenuSavePlatform final {
public:
    WiiUHomeMenuSavePlatform() = default;
    WiiUHomeMenuSavePlatform(const WiiUHomeMenuSavePlatform&) = delete;
    WiiUHomeMenuSavePlatform& operator=(const WiiUHomeMenuSavePlatform&) = delete;

    [[nodiscard]] bool ReadActiveSave(
        WiiUHomeMenuSaveSnapshot& snapshot,
        std::string& error);

    [[nodiscard]] bool CreateManualBackup(
        const WiiUHomeMenuSaveSnapshot& snapshot,
        std::string& createdPath,
        std::string& error) const;

    [[nodiscard]] bool ListBackups(
        std::vector<SaveBackupEntry>& entries,
        std::string& error) const;

    [[nodiscard]] bool RestoreBackup(
        const WiiUHomeMenuSaveSnapshot& snapshot,
        const std::string& backupPath,
        bool createAutomaticBackup,
        std::size_t maxAutomaticBackups,
        std::string& automaticBackupPath,
        std::string& error) const;

    [[nodiscard]] ApplyTransactionResult ApplyPreview(
        const WiiUHomeMenuSaveSnapshot& snapshot,
        const SaveModel& model,
        const PlanResult& plan,
        const SortOptions& options) const;

    [[nodiscard]] static const char* BackupDirectory() noexcept;
};

} // namespace titles_sorter

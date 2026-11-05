#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace titles_sorter {

enum class SaveBackupKind { Automatic, Manual };

struct SaveBackupEntry {
    std::string fileName;
    bool automatic{};
};

// File-level backup/restore service. It copies the active save byte-for-byte;
// it never serializes or reconstructs the parsed model.
class HomeMenuSaveBackupManager final {
public:
    [[nodiscard]] bool CreateBackup(const std::string& activeSavePath,
                                    const std::string& backupDirectory,
                                    SaveBackupKind kind,
                                    std::size_t maxAutomaticBackups,
                                    std::string& createdPath,
                                    std::string& error) const;

    [[nodiscard]] bool ListBackups(const std::string& backupDirectory,
                                   std::vector<SaveBackupEntry>& entries,
                                   std::string& error) const;

    // Restores the preserved original when startup detects an interrupted Restore.
    [[nodiscard]] bool RecoverInterruptedRestore(const std::string& activeSavePath,
                                                 std::string& error) const;

    // Restore validates the backup and staged copy before moving the active
    // save aside. When enabled, an automatic backup of the current save is
    // created first; Restore is aborted if that safety backup cannot be made.
    // The selected backup is read before retention runs, so it cannot be pruned
    // out from under an in-progress Restore.
    [[nodiscard]] bool RestoreBackup(const std::string& backupPath,
                                     const std::string& activeSavePath,
                                     const std::string& backupDirectory,
                                     bool createAutomaticBackup,
                                     std::size_t maxAutomaticBackups,
                                     std::string& automaticBackupPath,
                                     std::string& error) const;
};

} // namespace titles_sorter

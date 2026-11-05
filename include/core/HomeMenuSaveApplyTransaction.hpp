#pragma once

#include "core/TitlesSorterCore.hpp"

#include <cstddef>
#include <string>

namespace titles_sorter {

struct ApplyTransactionOptions {
    bool createAutomaticBackup{true};
    std::size_t maxAutomaticBackups{10U};
};

struct ApplyTransactionResult {
    bool applied{};
    bool backupCreated{};
    std::string backupPath;
    std::string error;
};

// Applies a previewed layout with validation and rollback staging. The active
// save is never opened for writing until the candidate and optional backup
// have both been verified.
class HomeMenuSaveApplyTransaction final {
public:
    // Restores the preserved original when an earlier Apply was interrupted.
    // Call before reading/parsing the active save during application startup.
    [[nodiscard]] bool RecoverInterruptedApply(
        const std::string& activeSavePath, std::string& error) const;

    [[nodiscard]] ApplyTransactionResult Apply(
        const SaveModel& model, const PlanResult& plan,
        const std::string& activeSavePath, const std::string& backupDirectory,
        const ApplyTransactionOptions& options) const;
};

} // namespace titles_sorter

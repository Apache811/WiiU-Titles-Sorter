#pragma once

#include "core/TitlesSorterCore.hpp"

#include <string>

namespace titles_sorter {

// Versioned, human-readable settings persistence. The store writes only when
// the serialized options differ from the last successfully loaded/saved state.
class TitlesSorterSettingsStore final {
public:
    [[nodiscard]] bool Load(const std::string& path, SortOptions& options,
                            std::string& error);
    [[nodiscard]] bool SaveIfChanged(const std::string& path,
                                     const SortOptions& options,
                                     std::string& error);

private:
    std::string lastSavedContent_;
    bool hasKnownContent_{};
};

} // namespace titles_sorter

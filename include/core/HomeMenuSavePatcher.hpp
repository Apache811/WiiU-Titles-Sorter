#pragma once

#include "core/TitlesSorterCore.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace titles_sorter {

// Builds a candidate save in memory. It does not access the filesystem and
// never mutates SaveModel::originalBytes.
class HomeMenuSavePatcher final {
public:
    [[nodiscard]] bool BuildPatchedSave(const SaveModel& model,
                                        const PlanResult& plan,
                                        std::vector<std::uint8_t>& patchedBytes,
                                        std::string& error) const;
};

} // namespace titles_sorter

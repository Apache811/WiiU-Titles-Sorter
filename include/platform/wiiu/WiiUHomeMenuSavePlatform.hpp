#pragma once

#include "core/WiiUHomeMenuSaveReader.hpp"

#include <string>

namespace titles_sorter {

// Wii U implementation of the read-only save acquisition pipeline.
// Call only after WHBProcInit() has completed. This class never writes files.
class WiiUHomeMenuSavePlatform final {
public:
    WiiUHomeMenuSavePlatform() = default;
    WiiUHomeMenuSavePlatform(const WiiUHomeMenuSavePlatform&) = delete;
    WiiUHomeMenuSavePlatform& operator=(const WiiUHomeMenuSavePlatform&) = delete;

    [[nodiscard]] bool ReadActiveSave(
        WiiUHomeMenuSaveSnapshot& snapshot,
        std::string& error);
};

} // namespace titles_sorter

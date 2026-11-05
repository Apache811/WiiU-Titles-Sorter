#pragma once

#include "ui/TitlesSorterUI.hpp"

#include <array>
#include <cstddef>

namespace titles_sorter {

struct UiInputBatch {
    std::array<UiInput, 10> events{};
    std::size_t count{};
};

// Converts VPAD trigger edges into platform-independent UI events.
// VPAD's trigger mask is edge-based, so held buttons are not repeated here.
class WiiUInputAdapter {
public:
    [[nodiscard]] UiInputBatch Poll() const noexcept;
};

} // namespace titles_sorter

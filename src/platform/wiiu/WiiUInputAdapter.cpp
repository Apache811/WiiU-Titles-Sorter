#include "platform/wiiu/WiiUInputAdapter.hpp"

#include <vpad/input.h>

namespace titles_sorter {

UiInputBatch WiiUInputAdapter::Poll() const noexcept {
    UiInputBatch batch{};
    VPADStatus status{};
    VPADReadError error{};
    if (VPADRead(VPAD_CHAN_0, &status, 1, &error) <= 0) return batch;

    const std::uint32_t trigger = status.trigger;
    const auto append = [&batch, trigger](std::uint32_t mask, UiInput event) {
        if ((trigger & mask) != 0U && batch.count < batch.events.size())
            batch.events[batch.count++] = event;
    };

    append(VPAD_BUTTON_UP, UiInput::Up);
    append(VPAD_BUTTON_DOWN, UiInput::Down);
    append(VPAD_BUTTON_LEFT, UiInput::Left);
    append(VPAD_BUTTON_RIGHT, UiInput::Right);
    append(VPAD_BUTTON_A, UiInput::Confirm);
    append(VPAD_BUTTON_B, UiInput::Back);
    append(VPAD_BUTTON_ZL, UiInput::Zl);
    append(VPAD_BUTTON_ZR, UiInput::Zr);
    append(VPAD_BUTTON_L, UiInput::L);
    append(VPAD_BUTTON_R, UiInput::R);
    return batch;
}

} // namespace titles_sorter

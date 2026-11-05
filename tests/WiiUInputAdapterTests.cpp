#include "platform/wiiu/WiiUInputAdapter.hpp"
#include <vpad/input.h>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <iostream>

using namespace titles_sorter;

int main() {
    WiiUInputAdapter adapter;
    SetVpadMockState(0U);
    UiInputBatch empty = adapter.Poll();
    assert(empty.count == 0U);

    SetVpadMockState(VPAD_BUTTON_UP | VPAD_BUTTON_A | VPAD_BUTTON_ZR | VPAD_BUTTON_HOME);
    UiInputBatch batch = adapter.Poll();
    assert(batch.count == 3U);
    assert(batch.events[0] == UiInput::Up);
    assert(batch.events[1] == UiInput::Confirm);
    assert(batch.events[2] == UiInput::Zr);

    SetVpadMockState(VPAD_BUTTON_ZL);
    batch = adapter.Poll();
    assert(batch.count == 1U);
    assert(batch.events[0] == UiInput::Zl);

    SetVpadMockState(VPAD_BUTTON_LEFT | VPAD_BUTTON_RIGHT | VPAD_BUTTON_L | VPAD_BUTTON_R);
    batch = adapter.Poll();
    assert(batch.count == 4U);
    assert(batch.events[0] == UiInput::Left);
    assert(batch.events[1] == UiInput::Right);
    assert(batch.events[2] == UiInput::L);
    assert(batch.events[3] == UiInput::R);

    SetVpadMockState(VPAD_BUTTON_B, 0);
    batch = adapter.Poll();
    assert(batch.count == 0U);

    std::cout << "Wii U input adapter tests passed.\n";
    return 0;
}

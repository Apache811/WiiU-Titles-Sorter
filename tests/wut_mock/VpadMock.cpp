#include <vpad/input.h>

namespace {
std::uint32_t gTrigger{};
int gReadCount{1};
}

void SetVpadMockState(std::uint32_t trigger, int readCount) {
    gTrigger = trigger;
    gReadCount = readCount;
}

int VPADRead(int channel, VPADStatus* status, int count, VPADReadError* error) {
    if (channel != VPAD_CHAN_0 || status == nullptr || count <= 0 || error == nullptr) return 0;
    *error = VPAD_READ_SUCCESS;
    status->trigger = gTrigger;
    return gReadCount;
}

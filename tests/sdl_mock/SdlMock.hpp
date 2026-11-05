#pragma once
#include <cstddef>

struct SdlMockStats {
    std::size_t clearCalls{};
    std::size_t lineCalls{};
    std::size_t copyCalls{};
    std::size_t presentCalls{};
    std::size_t createdTextures{};
    std::size_t destroyedTextures{};
    int logicalWidth{};
    int logicalHeight{};
};

SdlMockStats& GetSdlMockStats();
void ResetSdlMockStats();

#pragma once
#include <cstdint>

constexpr int VPAD_CHAN_0 = 0;
constexpr std::uint32_t VPAD_BUTTON_UP = 1U << 0U;
constexpr std::uint32_t VPAD_BUTTON_DOWN = 1U << 1U;
constexpr std::uint32_t VPAD_BUTTON_LEFT = 1U << 2U;
constexpr std::uint32_t VPAD_BUTTON_RIGHT = 1U << 3U;
constexpr std::uint32_t VPAD_BUTTON_A = 1U << 4U;
constexpr std::uint32_t VPAD_BUTTON_B = 1U << 5U;
constexpr std::uint32_t VPAD_BUTTON_ZR = 1U << 6U;
constexpr std::uint32_t VPAD_BUTTON_ZL = 1U << 10U;
constexpr std::uint32_t VPAD_BUTTON_L = 1U << 7U;
constexpr std::uint32_t VPAD_BUTTON_R = 1U << 8U;
constexpr std::uint32_t VPAD_BUTTON_HOME = 1U << 9U;

struct VPADStatus { std::uint32_t trigger{}; };
enum VPADReadError : int { VPAD_READ_SUCCESS = 0, VPAD_READ_ERROR = 1 };
int VPADRead(int channel, VPADStatus* status, int count, VPADReadError* error);
void SetVpadMockState(std::uint32_t trigger, int readCount = 1);

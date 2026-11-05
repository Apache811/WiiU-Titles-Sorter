#include "McpMock.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

namespace {
std::vector<MCPTitleListType> gTitles;
}

void SetMockMcpTitles(const std::vector<MCPTitleListType>& titles) {
    gTitles = titles;
}

void ResetMockMcpTitles() {
    gTitles.clear();
}

extern "C" std::int32_t MCP_Open() {
    return 1;
}

extern "C" std::int32_t MCP_Close(std::int32_t handle) {
    return handle == 1 ? 0 : -1;
}

namespace {
bool IsNativeInstalledTitle(const MCPTitleListType& title) {
    return (title.titleId >> 32U) != 0x0005000FULL;
}

std::vector<MCPTitleListType> NativeTitles() {
    std::vector<MCPTitleListType> native;
    native.reserve(gTitles.size());
    for (const MCPTitleListType& title : gTitles) {
        if (IsNativeInstalledTitle(title)) native.push_back(title);
    }
    return native;
}
}

extern "C" MCPError MCP_TitleCount(std::int32_t handle) {
    const std::size_t count = NativeTitles().size();
    if (handle != 1 || count > static_cast<std::size_t>(std::numeric_limits<MCPError>::max())) {
        return -1;
    }
    return static_cast<MCPError>(count);
}

extern "C" MCPError MCP_TitleList(std::int32_t handle,
                                    std::uint32_t* count,
                                    MCPTitleListType* list,
                                    std::uint32_t bytes) {
    if (handle != 1 || count == nullptr || (list == nullptr && !gTitles.empty())) return -1;
    const std::size_t capacity = bytes / sizeof(MCPTitleListType);
    const std::size_t copied = std::min({gTitles.size(), capacity, static_cast<std::size_t>(*count)});
    if (copied != 0U) std::memcpy(list, gTitles.data(), copied * sizeof(MCPTitleListType));
    *count = static_cast<std::uint32_t>(copied);
    return 0;
}

extern "C" MCPError MCP_TitleListByDeviceType(std::int32_t handle,
                                                MCPDeviceType deviceType,
                                                std::uint32_t* count,
                                                MCPTitleListType* list,
                                                std::uint32_t bytes) {
    if (handle != 1 || count == nullptr || deviceType != MCP_DEVICE_TYPE_AUTO) return -1;
    const std::vector<MCPTitleListType> native = NativeTitles();
    if (list == nullptr && !native.empty()) return -1;
    const std::size_t capacity = bytes / sizeof(MCPTitleListType);
    const std::size_t copied = std::min({native.size(), capacity, static_cast<std::size_t>(*count)});
    if (copied != 0U) std::memcpy(list, native.data(), copied * sizeof(MCPTitleListType));
    *count = static_cast<std::uint32_t>(copied);
    return 0;
}

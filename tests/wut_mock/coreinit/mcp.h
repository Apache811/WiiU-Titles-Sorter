#pragma once

#include <cstddef>
#include <cstdint>

// Layout copied from devkitPro/wut include/coreinit/mcp.h.
// This host-only declaration allows ABI/layout checks without pretending to
// replace the real WUT header during a console build.
typedef std::int32_t MCPError;

typedef enum MCPDeviceType {
    MCP_DEVICE_TYPE_AUTO = 1,
    MCP_DEVICE_TYPE_ODD = 2,
    MCP_DEVICE_TYPE_MLC = 3,
    MCP_DEVICE_TYPE_USB = 4,
} MCPDeviceType;

typedef enum MCPAppType {
    MCP_APP_TYPE_GAME_UPDATE = 0x0800001B,
    MCP_APP_TYPE_GAME_DLC = 0x0800000E,
    MCP_APP_TYPE_GAME = static_cast<std::int32_t>(0x80000000U),
} MCPAppType;

#pragma pack(push, 1)
typedef struct MCPTitleListType {
    std::uint64_t titleId;       // 0x00
    std::uint32_t groupId;       // 0x08
    char path[56];               // 0x0C
    MCPAppType appType;          // 0x44
    std::uint16_t titleVersion;  // 0x48
    std::uint64_t osVersion;     // 0x4A
    std::uint32_t sdkVersion;    // 0x52
    char indexedDevice[10];      // 0x56
    std::uint8_t unk0x60;        // 0x60
} MCPTitleListType;
#pragma pack(pop)

static_assert(offsetof(MCPTitleListType, titleId) == 0x00);
static_assert(offsetof(MCPTitleListType, path) == 0x0C);
static_assert(offsetof(MCPTitleListType, appType) == 0x44);
static_assert(offsetof(MCPTitleListType, indexedDevice) == 0x56);
static_assert(sizeof(MCPTitleListType) == 0x61);

extern "C" {
std::int32_t MCP_Open();
std::int32_t MCP_Close(std::int32_t handle);
MCPError MCP_TitleCount(std::int32_t handle);
MCPError MCP_TitleList(std::int32_t handle,
                       std::uint32_t* count,
                       MCPTitleListType* list,
                       std::uint32_t bytes);
MCPError MCP_TitleListByDeviceType(std::int32_t handle,
                                   MCPDeviceType deviceType,
                                   std::uint32_t* count,
                                   MCPTitleListType* list,
                                   std::uint32_t bytes);
}

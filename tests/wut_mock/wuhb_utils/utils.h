#pragma once

#include <cstdint>

extern "C" {
enum WUHBUtilsStatus : std::int32_t {
    WUHB_UTILS_RESULT_SUCCESS = 0,
    WUHB_UTILS_RESULT_MODULE_NOT_FOUND = -1,
    WUHB_UTILS_RESULT_UNKNOWN_ERROR = -256
};
enum BundleSource : std::int32_t {
    BundleSource_FileDescriptor = 0,
    BundleSource_FileDescriptor_CafeOS = 1
};
WUHBUtilsStatus WUHBUtils_InitLibrary();
WUHBUtilsStatus WUHBUtils_MountBundle(const char*, const char*, BundleSource, std::int32_t*);
WUHBUtilsStatus WUHBUtils_UnmountBundle(const char*, std::int32_t*);
WUHBUtilsStatus WUHBUtils_ReadWholeFile(const char*, std::uint8_t**, std::uint32_t*);
}

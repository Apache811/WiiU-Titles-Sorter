#pragma once

#include <coreinit/debug.h>
#include <whb/log.h>
#include <sys/time.h>
#include <time.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

inline void TitleSorterLog(const char* component, const char* message)
{
    timeval tv{};
    gettimeofday(&tv, nullptr);

    const time_t seconds = tv.tv_sec;
    const tm* localTime = localtime(&seconds);

    char timestamp[20] = "00:00:00:0000";
    if (localTime != nullptr && strftime(timestamp, sizeof(timestamp), "%H:%M:%S", localTime) > 0)
    {
        const size_t length = std::strlen(timestamp);
        const unsigned int fractional = static_cast<unsigned int>((tv.tv_usec / 100) % 10000);
        std::snprintf(timestamp + length, sizeof(timestamp) - length, ":%04u", fractional);
    }

    OSReport("[%s] [TitlesSorter] - {%s} %s\n", timestamp, component, message);
    WHBLogPrintf("[%s] [TitlesSorter] - {%s} %s", timestamp, component, message);
}

inline void TitleSorterLogf(const char* component, const char* format, ...)
{
    char message[512]{};
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    TitleSorterLog(component, message);
}

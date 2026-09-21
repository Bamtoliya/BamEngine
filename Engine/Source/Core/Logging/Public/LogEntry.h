#pragma once

#include <chrono>
#include <cstdint>
#include <deque>
#include <string>
#include <thread>

namespace Engine
{
    enum class ELogLevel : std::uint8_t
    {
        Trace,
        Info,
        Warn,
        Error,
        Fatal
    };

    struct LogEntry
    {
        std::chrono::system_clock::time_point Timestamp{};
        ELogLevel Level = ELogLevel::Info;

        // 큐에 보관하므로 문자열을 직접 소유합니다.
        std::string Category;
        std::string Message;
        std::string FileName;

        std::uint_least32_t Line = 0;
        std::thread::id ThreadId{};
    };

    // 한 번에 가져오는 로그와 유실 개수입니다.
    struct LogBatch
    {
        std::deque<LogEntry> Entries;
        std::uint64_t DroppedCount = 0;
    };
}
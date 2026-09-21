#include "Logger.h"

#pragma push_macro("new")
#undef new

#include <ctime>
#include <fmt/chrono.h>

#pragma pop_macro("new")

struct LoggerState
{
    // 기존 파일·터미널 출력 보호
    std::mutex OutputMutex;
    std::ofstream File;
    std::atomic<Engine::ELogLevel> MinLevel{
        Engine::ELogLevel::Trace
    };

    // Console 전달용 큐 보호
    std::mutex QueueMutex;
    std::deque<Engine::LogEntry> PendingEntries;

    static constexpr std::size_t MaxPendingEntries = 4096;
    std::uint64_t DroppedCount = 0;
};

LoggerState& GetLoggerState()
{
    static LoggerState state;
    return state;
}

const char* GetLevelName(ELogLevel level)
{
	switch (level)
	{
	case ELogLevel::Trace: return "TRACE";
	case ELogLevel::Info:  return "INFO";
	case ELogLevel::Warn:  return "WARN";
	case ELogLevel::Error: return "ERROR";
	case ELogLevel::Fatal: return "FATAL";
	default:               return "UNKNOWN";
	}
}


ENGINE_API bool Logger::Initialize(const std::filesystem::path& filePath)
{
	auto& state = GetLoggerState();
	std::lock_guard lock(state.OutputMutex);

	if (state.File.is_open())
		return true;

	error_code error;
	const auto parentPath = filePath.parent_path();

	if (!parentPath.empty() && !std::filesystem::exists(parentPath))
	{
		if (!std::filesystem::create_directories(parentPath, error))
		{
			fmt::print(stderr, "Failed to create log directory: {}\n", error.message());
			return false;
		}
	}

	state.File.clear();
	state.File.open(filePath, std::ios::out | std::ios::app);

	if (!state.File.is_open())
	{
		fmt::print(stderr, "Failed to open log file: {}\n", filePath.string());
		return false;
	}

	return true;
}

ENGINE_API void Logger::Shutdown()
{
	auto& state = GetLoggerState();
	std::lock_guard lock(state.OutputMutex);

	if (state.File.is_open())
	{
		state.File.flush();
		state.File.close();
	}
}

ENGINE_API void Logger::SetMinLevel(ELogLevel level)
{
	GetLoggerState().MinLevel.store(level, std::memory_order_relaxed);
}

ENGINE_API bool Logger::IsEnabled(ELogLevel level)
{
	const auto& minLevel = GetLoggerState().MinLevel.load(std::memory_order_relaxed);
	return level >= minLevel;
}

ENGINE_API LogBatch Logger::DrainPendingEntries()
{
    auto& state = GetLoggerState();
    LogBatch batch;

    {
        std::lock_guard lock(state.QueueMutex);

        // 각 로그를 복사하지 않고 컨테이너를 교환합니다.
        batch.Entries.swap(state.PendingEntries);

        batch.DroppedCount = state.DroppedCount;
        state.DroppedCount = 0;
    }

    return batch;
}

ENGINE_API void Logger::Write(ELogLevel level, string_view category, string_view message, source_location location)
{
    auto& state = GetLoggerState();

    // 한 메시지의 터미널·파일 출력을 하나의 작업으로 보호합니다.
    std::lock_guard lock(state.OutputMutex);

    const auto now = std::chrono::system_clock::now();

    Engine::LogEntry entry;
    entry.Timestamp = now;
    entry.Level = level;
    entry.Category = std::string(category);
    entry.Message = std::string(message);
    entry.FileName = location.file_name();
    entry.Line = location.line();
    entry.ThreadId = std::this_thread::get_id();

    {
        std::lock_guard queueLock(state.QueueMutex);

        state.PendingEntries.push_back(std::move(entry));

        if (state.PendingEntries.size() >
            LoggerState::MaxPendingEntries)
        {
            state.PendingEntries.pop_front();
            ++state.DroppedCount;
        }
    }

    const auto seconds =
        std::chrono::time_point_cast<std::chrono::seconds>(now);

    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now - seconds).count();

    const auto time =
        std::chrono::system_clock::to_time_t(seconds);

    std::tm localTime{};

    if (::localtime_s(&localTime, &time) != 0)
    {
        std::fputs("[Logger] Failed to convert local time.\n", stderr);
        return;
    }

    std::ostringstream threadId;
    threadId << std::this_thread::get_id();

    const auto line = fmt::format(
        "[{:%Y-%m-%d %H:%M:%S}.{:03}]"
        "[{}][{}][Thread:{}] {} ({}:{})\n",
        localTime,
        milliseconds,
        GetLevelName(level),
        category,
        threadId.str(),
        message,
        location.file_name(),
        location.line());

    // 첫 버전은 모든 레벨을 stderr로 모아 출력 순서를 유지합니다.
    std::fwrite(line.data(), 1, line.size(), stderr);
    std::fflush(stderr);

    if (state.File.is_open())
    {
        state.File.write(
            line.data(),
            static_cast<std::streamsize>(line.size()));

        // 첫 버전은 즉시 기록을 확인할 수 있도록 매번 flush합니다.
        state.File.flush();

        if (!state.File)
        {
            std::fputs(
                "[Logger] File write failed; "
                "file output disabled.\n",
                stderr);

            state.File.close();
        }
    }
}

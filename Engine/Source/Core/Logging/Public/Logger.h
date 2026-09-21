#pragma once

#include "Engine_API.h"
#include "Macro.h"

#include <source_location>
#include <utility>
#include <string_view>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <fstream>
#include <fmt/format.h>

#include "LogEntry.h"

BEGIN(Engine)
class Logger final
{
public:
	Logger() = delete;
public:
	ENGINE_API static bool Initialize(const std::filesystem::path& filePath);
	ENGINE_API static void Shutdown();

	ENGINE_API static void SetMinLevel(ELogLevel level);
	ENGINE_API static bool IsEnabled(ELogLevel level);
	ENGINE_API static LogBatch DrainPendingEntries();

	template<typename... Args>
	static void Log(ELogLevel level, std::string_view category, std::source_location location, fmt::format_string<Args...> format, Args&&... args)
	{
		if (!IsEnabled(level))
			return;
		const auto message = fmt::format(format, std::forward<Args>(args)...);

		Write(level, category, message, location);
	}

private:
	ENGINE_API static void Write(ELogLevel, std::string_view, std::string_view, std::source_location);
};
END

/*****************************************************************************/

#define BAM_LOG(Level, Category, Format, ...)                         \
    do                                                               \
    {                                                                \
        if (::Engine::Logger::IsEnabled(::Engine::ELogLevel::Level))   \
        {                                                            \
            ::Engine::Logger::Log(                                   \
                ::Engine::ELogLevel::Level,                            \
                Category,                                            \
                std::source_location::current(),                     \
                Format __VA_OPT__(,) __VA_ARGS__);                    \
        }                                                            \
    } while (false)

#define ENGINE_LOG_TRACE(Format, ...) BAM_LOG(Trace, "Engine", Format __VA_OPT__(,) __VA_ARGS__)
#define ENGINE_LOG_INFO(Format, ...) BAM_LOG(Info, "Engine", Format __VA_OPT__(,) __VA_ARGS__)
#define ENGINE_LOG_WARN(Format, ...) BAM_LOG(Warn, "Engine", Format __VA_OPT__(,) __VA_ARGS__)
#define ENGINE_LOG_ERROR(Format, ...) BAM_LOG(Error, "Engine", Format __VA_OPT__(,) __VA_ARGS__)
#define ENGINE_LOG_FATAL(Format, ...) BAM_LOG(Fatal, "Engine", Format __VA_OPT__(,) __VA_ARGS__)

/*****************************************************************************/
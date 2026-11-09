#pragma once

#include "Engine_API.h"
#include "Macro.h"
#include <filesystem>
#include <string>
#include <functional>

BEGIN(Engine)

class CrashHandler final
{
public:
	CrashHandler() = delete;

	using EmergencyCallback = void(*)();

	ENGINE_API static void Initialize(const std::filesystem::path& crashDumpDirectory = L"Logs/Crashes");
	ENGINE_API static void Shutdown();

	ENGINE_API static void SetActiveScenePath(const std::string& scenePath);
	ENGINE_API static void SetCustomContext(const std::string& key, const std::string& value);

	ENGINE_API static void RegisterEmergencyCallback(EmergencyCallback callback);

	ENGINE_API static const std::filesystem::path& GetCrashDumpDirectory();
	ENGINE_API static const std::filesystem::path& GetLastDumpPath();
	ENGINE_API static const std::filesystem::path& GetLastReportPath();

	// 검증 및 테스트용 고의 크래시 트리거
	ENGINE_API static void TriggerCrashForTesting();
};

END

#pragma once

#include "Editor_Includes.h"
#include "ShaderCompiler.h"
#include <efsw/efsw.hpp>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>

BEGIN(Editor)

struct ShaderNotification
{
	std::string Title;
	std::string Message;
	bool IsError = false;
	float RemainingTime = 5.0f;
};

class ShaderHotReloadManager final : public Base
{
	DECLARE_SINGLETON(ShaderHotReloadManager)

private:
	ShaderHotReloadManager() = default;
	virtual ~ShaderHotReloadManager() = default;
	EResult Initialize(void* arg = nullptr);

public:
	void Free() override;

	void Update();
	void RequestRecompileAll() { m_PendingRecompileAll = true; }
	void RecompileAll();
	void NotifyFileModified(const std::filesystem::path& filePath);

	// ImGui 렌더링 루프에서 호출할 우측 하단 토스트 알림 창
	void DrawNotificationUI();
	void AddNotification(const std::string& title, const std::string& msg, bool isError, float duration = 5.0f);

	[[nodiscard]] bool IsAutoReloadEnabled() const { return m_AutoReload; }
	void SetAutoReload(bool enable) { m_AutoReload = enable; }
	[[nodiscard]] bool& GetAutoReloadRef() { return m_AutoReload; }
	[[nodiscard]] const std::filesystem::path& GetWatchDirectory() const { return m_WatchDirectory; }

private:
	void ProcessModifiedFile(const std::filesystem::path& hlslPath);
	std::filesystem::path ResolveShaderSourceDirectory();

private:
	efsw::FileWatcher* m_FileWatcher = nullptr;
	efsw::FileWatchListener* m_FileListener = nullptr;
	efsw::WatchID m_WatchID = 0;
	std::filesystem::path m_WatchDirectory;

	std::atomic<bool> m_PendingRecompileAll = false;
	std::mutex m_QueueMutex;
	std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_PendingFiles;
	bool m_AutoReload = true;

	std::vector<ShaderNotification> m_Notifications;
};

END
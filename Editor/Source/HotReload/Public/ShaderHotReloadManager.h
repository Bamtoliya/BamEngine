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

class ShaderHotReloadManager final : public Base
{
	DECLARE_SINGLETON(ShaderHotReloadManager)

private:
	ShaderHotReloadManager() = default;
	virtual ~ShaderHotReloadManager() = default;
	EResult Initialize(void* arg = nullptr);

public:
	void Free() override;

	// 매 프레임 메인 루프에서 변경 큐를 디바운스 확인 후 컴파일 & 핫스왑
	void Update();

	// UI 버튼 등에서 호출하는 안전한 비동기 재컴파일 요청 (다음 프레임 렌더 시작 전 실행)
	void RequestRecompileAll() { m_PendingRecompileAll = true; }

	// 전체 셰이더 일괄 재컴파일 및 핫스왑 (툴바 버튼 또는 .hlsli 수정 시)
	void RecompileAll();

	void NotifyFileModified(const std::filesystem::path& filePath);

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
};

END
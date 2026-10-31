#pragma once

#include "HotReloaderInterface.h"
#include "ShaderCompiler.h"
#include <efsw/efsw.hpp>

BEGIN(Editor)

class ShaderHotReloadManager final : public IHotReloader
{
#pragma region Constructor&Destructor
private:
	ShaderHotReloadManager() = default;
	virtual ~ShaderHotReloadManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static ShaderHotReloadManager* Create();
	void Free() override;
#pragma endregion

public:
	void Update() override;
	void RequestRecompileAll() { m_PendingRecompileAll = true; }
	void RecompileAll();
	void RecompileFile(const std::filesystem::path& hlslPath) { ProcessModifiedFile(hlslPath); }
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
};

END
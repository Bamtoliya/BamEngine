#pragma once

#include "HotReloaderInterface.h"
#include <efsw/efsw.hpp>
#include <filesystem>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <vector>
#include <string>

BEGIN(Editor)

class AssetHotReloadManager final : public IHotReloader
{
#pragma region Constructor & Destructor
private:
	AssetHotReloadManager() = default;
	virtual ~AssetHotReloadManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static AssetHotReloadManager* Create(void* arg = nullptr);
	void Free() override;
#pragma endregion
public:
	void Update() override;
private:
	void ProcessModifiedTexture(const std::filesystem::path& texturePath);
	void ProcessModifiedMaterial(const std::filesystem::path& materialPath);
	void ProcessModifiedMesh(const std::filesystem::path& meshPath);
	void ProcessModifiedLocalization(const std::filesystem::path& locPath);
	std::filesystem::path ResolveTextureDirectory();

private:
	efsw::FileWatcher* m_FileWatcher = nullptr;
	efsw::FileWatchListener* m_FileListener = nullptr;
	efsw::WatchID m_WatchID = 0;
	std::filesystem::path m_WatchDirectory;
};

END
#pragma once

#include "HotReloaderInterface.h"
#include <efsw/efsw.hpp>

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
	void RecordMeshFileState(const std::filesystem::path& filePath);
private:
	void ProcessModifiedTexture(const std::filesystem::path& texturePath);
	void ProcessModifiedMaterial(const std::filesystem::path& materialPath);
	void ProcessModifiedMesh(const std::filesystem::path& meshPath);
	void ProcessModifiedLocalization(const std::filesystem::path& locPath);
	std::filesystem::path ResolveTextureDirectory();

private:
	struct MeshFileState
	{
		std::filesystem::file_time_type WriteTime;
		std::uintmax_t Size = 0;

		bool operator==(const MeshFileState&) const = default;
	};

	static bool ReadMeshFileState(const std::filesystem::path& filePath, MeshFileState& state);
private:
	efsw::FileWatcher* m_FileWatcher = nullptr;
	efsw::FileWatchListener* m_FileListener = nullptr;
	efsw::WatchID m_WatchID = 0;
	efsw::WatchID m_EngineWatchID = 0;
	std::filesystem::path m_WatchDirectory;
	std::filesystem::path m_EngineWatchDirectory;
	// 메인 스레드에서만 접근합니다.
	std::unordered_map<std::wstring, MeshFileState> m_MeshFileStates;
};

END
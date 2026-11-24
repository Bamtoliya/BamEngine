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
	void RequestProjectLocalizationImport(std::filesystem::path source);
	void RequestProjectLocalizationReload();
private:
	void ImportProjectLocalization(const std::filesystem::path& source);
	void ReloadProjectLocalization();
	void ProcessModifiedTexture(const std::filesystem::path& texturePath);
	void ProcessModifiedMaterial(const std::filesystem::path& materialPath);
	void ProcessModifiedMesh(const std::filesystem::path& meshPath);
	void ProcessModifiedTimeline(const std::filesystem::path& timelinePath);
	void ProcessModifiedTerrainLayer(const std::filesystem::path& layerPath);
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
	std::vector<efsw::WatchID> m_WatchIDs;
	std::vector<std::filesystem::path> m_WatchedDirectories;
	std::unordered_map<std::wstring, MeshFileState> m_MeshFileStates;
	std::filesystem::path m_PendingProjectLocaleImport;
	bool m_ProjectLocaleReloadRequested = false;
};

END
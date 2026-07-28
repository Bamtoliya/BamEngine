#pragma once

#include "Editor_Includes.h"
#include <mutex>
#include <vector>
#include <unordered_set>
#include <optional>

BEGIN(Editor)
class AssetCache : public Base
{
	DECLARE_SINGLETON(AssetCache)

#pragma region Constructor&Destructor
private:
	AssetCache() = default;
	virtual ~AssetCache() = default;
	EResult Initialize(void* arg = nullptr);
	virtual void Free() override;

	void Update();
#pragma endregion

#pragma region Thumbnail Management
public:
	void* GetThumbnail(const filesystem::path& assetPath);

	void ClearCache(const filesystem::path& assetPath);
	void ClearAll();

private:
	struct ThumbnailUploadTask {
		std::string AssetPath;
		int32 Width, Height;
		std::vector<uint8_t> Data;
	};

	std::optional<ThumbnailUploadTask> LoadImageThumbnail(const filesystem::path& assetPath);
	std::optional<ThumbnailUploadTask> LoadModelThumbnail(const filesystem::path& assetPath);
#pragma endregion

#pragma region Member Variables
private:
	unordered_map<string, void*> m_ThumbnailCache;
	unordered_map<string, RHITexture*> m_ThumbnailTextures;
	std::unordered_set<std::string> m_LoadingPaths;
	
	std::mutex m_UploadMutex;
	std::vector<ThumbnailUploadTask> m_UploadTasks;
#pragma endregion



};
END
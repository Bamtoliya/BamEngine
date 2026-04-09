#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class AssetCache : public Base
{
	DECLARE_SINGLETON(AssetCache)

#pragma region Constructor&Destructor
private:
	AssetCache() = default;
	virtual ~AssetCache() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Thumbnail Management
public:
	void* GetThumbnail(const filesystem::path& assetPath);

	void ClearCache(const filesystem::path& assetPath);
	void ClearAll();

private:
	void* LoadImageThumbnail(const filesystem::path& assetPath);
	void* LoadModelThumbnail(const filesystem::path& assetPath);
#pragma endregion

#pragma region Member Variables
private:
	unordered_map<string, void*> m_ThumbnailCache;
	unordered_map<string, RHITexture*> m_ThumbnailTextures;
#pragma endregion



};
END
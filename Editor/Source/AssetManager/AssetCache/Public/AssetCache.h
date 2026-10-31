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
	virtual void Free() override;

public:
	void Update();
#pragma endregion

#pragma region Thumbnail Management
public:
	RHITexture* GetThumbnail(const filesystem::path& assetPath);
	// 어느 스레드에서든 호출할 수 있습니다.
	void RequestInvalidate(const filesystem::path& assetPath);
	void ClearCache(const filesystem::path& assetPath);
	void ClearAll();

private:
	struct ThumbnailUploadTask
	{
		std::string AssetPath;
		Engine::uint64 RequestId = 0;
		int32 Width = 0, Height = 0;
		std::vector<uint8_t> Data;
	};

	std::optional<ThumbnailUploadTask> LoadImageThumbnail(const filesystem::path& assetPath);
	std::optional<ThumbnailUploadTask> LoadModelThumbnail(const filesystem::path& assetPath);
#pragma endregion

#pragma region Member Variables
private:
	unordered_map<string, RHITexture*> m_ThumbnailCache;

	// 메인 스레드 전용 상태입니다.
	std::unordered_map<std::string, Engine::uint64> m_LoadingPaths;
	Engine::uint64 m_NextRequestId = 0;

	// 완료 결과와 무효화 요청만 이 뮤텍스로 보호합니다.
	std::mutex m_UploadMutex;
	std::vector<ThumbnailUploadTask> m_UploadTasks;
	std::unordered_set<std::string> m_PendingInvalidations;
#pragma endregion
};
END
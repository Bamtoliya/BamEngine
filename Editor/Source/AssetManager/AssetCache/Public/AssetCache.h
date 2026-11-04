#pragma once

#include "Editor_Includes.h"
#include <list>
#include <deque>

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

public:
	// F12 예산 및 상한 정책 상수
	static constexpr size_t MAX_CACHE_ENTRIES = 256;             // 최대 캐시 엔트리 수
	static constexpr size_t MAX_CACHE_BYTES = 64 * 1024 * 1024;   // 최대 VRAM 캐시 예산 (64 MB)
	static constexpr size_t MAX_UPLOADS_PER_FRAME = 4;           // 프레임당 최대 GPU 텍스처 생성 수
	static constexpr int32 MAX_THUMBNAIL_DIM = 128;              // 썸네일 최대 가로/세로 해상도

private:
	struct CacheEntry
	{
		RHITexture* Texture = nullptr;
		size_t ByteSize = 0;
		std::list<std::string>::iterator LruIt;
	};

	struct ThumbnailUploadTask
	{
		std::string AssetPath;
		Engine::uint64 RequestId = 0;
		int32 Width = 0, Height = 0;
		std::vector<uint8_t> Data;
	};

	std::optional<ThumbnailUploadTask> LoadImageThumbnail(const filesystem::path& assetPath);
	std::optional<ThumbnailUploadTask> LoadModelThumbnail(const filesystem::path& assetPath);

private:
	void ProcessPending3DThumbnail();
	void InsertToCache(const std::string& key, RHITexture* texture, size_t byteSize = 0);
	void EvictOldest();

	enum class ECaptureState : uint8
	{
		Idle,
		Rendering
	};
	struct Thumbnail3DRequest
	{
		std::string AssetPath;
		Engine::uint64 RequestId = 0;
	};

	std::queue<Thumbnail3DRequest> m_Pending3DQueue;
	std::string m_CurrentCapturingKey;
	Engine::uint64 m_CurrentCapturingRequestId = 0;
	ECaptureState m_CaptureState = ECaptureState::Idle;
#pragma endregion

#pragma region Member Variables
private:
	// LRU 캐시 및 바이트 예산 추적 상태
	std::unordered_map<std::string, CacheEntry> m_ThumbnailCache;
	std::list<std::string> m_LruList;
	size_t m_CurrentCacheBytes = 0;

	// 프레임당 업로드 스로틀링을 위한 메인 스레드 대기열
	std::deque<ThumbnailUploadTask> m_PendingUploadQueue;

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
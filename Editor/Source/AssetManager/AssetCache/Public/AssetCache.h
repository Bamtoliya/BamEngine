#pragma once

#include "Editor_Includes.h"
#include <list>
#include <deque>
#include <future>
#include <vector>

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

#pragma region Loop
public:
	void Update();
#pragma endregion

#pragma region Thumbnail Management
public:
	RHITexture* GetThumbnail(const filesystem::path& assetPath, Engine::Handle* spriteHandle = nullptr);

	// 어느 스레드에서든 호출할 수 있습니다.
	void RequestInvalidate(const filesystem::path& assetPath);
	void ClearCache(const filesystem::path& assetPath);
	void ClearAll();

public:
	// F12 예산 및 상한 정책 상수
	static constexpr size_t MAX_CACHE_ENTRIES = 1024;            // 최대 캐시 엔트리 수
	static constexpr size_t MAX_CACHE_BYTES = 128 * 1024 * 1024; // 최대 VRAM 캐시 예산 (128 MB)
	static constexpr size_t MAX_UPLOADS_PER_FRAME = 4;           // 프레임당 최대 GPU 텍스처 생성 수
	static constexpr int32 MAX_THUMBNAIL_DIM = 128;              // 썸네일 최대 가로/세로 해상도
	static constexpr size_t MAX_DECODE_WORKERS = 2;
	static constexpr size_t MAX_THUMBNAIL_SOURCE_BYTES = 32 * 1024 * 1024;
	static constexpr size_t MAX_THUMBNAIL_RGBA_BYTES = 64 * 1024 * 1024;
	static constexpr int32 MAX_THUMBNAIL_SOURCE_DIM = 8192;
	static constexpr size_t MAX_PENDING_3D_REQUESTS = 32;
	static constexpr size_t MAX_3D_REQUESTS_PER_FRAME = 1;
	static constexpr size_t MAX_RAW_MODEL_PARSE_BYTES = 4 * 1024 * 1024;
	static constexpr size_t MAX_COMPOSITE_MODEL_PARSE_BYTES = 64 * 1024;

private:
	struct CacheEntry
	{
		RHITexture* Texture = nullptr;
		Engine::Handle SpriteHandle;
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

	struct ThumbnailDecodeWorker
	{
		std::string AssetPath;
		Engine::uint64 RequestId = 0;
		std::future<std::optional<ThumbnailUploadTask>> Future;
	};

	static std::optional<ThumbnailUploadTask> LoadImageThumbnail(const filesystem::path& assetPath);
	void CollectFinishedThumbnailWorkers();
	std::optional<ThumbnailUploadTask> LoadModelThumbnail(const filesystem::path& assetPath);

private:
	void ProcessPending3DThumbnail();
	void InsertToCache(const std::string& key, RHITexture* texture, size_t byteSize = 0, Engine::Handle spriteHandle = {});
	void EvictOldest();
private:
	static std::filesystem::path GetDiskCacheDirectory();
	static std::filesystem::path CalculateDiskCachePath(const std::filesystem::path& assetPath);
	static std::optional<ThumbnailUploadTask> LoadThumbnailFromDiskCache(const std::filesystem::path& cachePath, const std::string& key);
	static bool SaveThumbnailToDiskCache(const std::filesystem::path& cachePath, int32 width, int32 height, const uint8_t* rgbaData);
	static bool SaveNegativeCacheToDisk(const std::filesystem::path& cachePath);
	static void InvalidateDiskCache(const std::filesystem::path& assetPath);
	static void PruneDiskCache(size_t maxBytes = 512 * 1024 * 1024);

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
	std::vector<Engine::Resource*> m_TempCaptureResources;
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

	// 메인 스레드에서 작업을 등록하고 완료 결과를 회수합니다.
	std::vector<ThumbnailDecodeWorker> m_DecodeWorkers;

	// 다른 스레드에서는 무효화 요청만 전달합니다.
	std::mutex m_UploadMutex;
	std::unordered_set<std::string> m_PendingInvalidations;
#pragma endregion
};
END
#pragma once
#include "AssetCache.h"
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include "Renderer.h"
#include "RHI.h"
#include <shlobj.h> // Windows Shell API
#include "AssetManager.h"
#include "RHITexture.h"
#include "ThumbnailRenderer.h"
#include "AssetWorkerHost.h"
#include "ResourceManager.h"
#include "MaterialInterface.h"
#include "Texture.h"
#include "Sprite.h"
#include "Model.h"
#include "Mesh.h"
#include "Vertex.h"
#include "ModelConverter.h"
#include <DirectXTex.h>
#include "Functions.h"
#include "PlatformPath.h"
#include <fstream>
#include <limits>
#include "MaterialInstance.h"
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>
#include <memory>

namespace
{
    bool IsPrefabThumbnail(std::string_view filename)
    {
        return filename.ends_with(".bamprefab") || filename.ends_with(".prefab") ||
            filename.ends_with(".bamprefab.json") || filename.ends_with(".prefab.json");
    }

    std::string ThumbnailCacheKey(const filesystem::path& path)
    {
        if (path.empty()) return {};

        const auto absolutePath = Engine::PlatformPath::ResolveProjectRelative(path);
        return Engine::WStrToStr(absolutePath.generic_wstring());
    }

    std::string ThumbnailResourceKey(const Engine::Resource* resource)
    {
        if (resource == nullptr) return {};

        const auto& path = resource->GetPath().empty() ? resource->GetKey() : resource->GetPath();
        return ThumbnailCacheKey(path);
    }

    void ExpandThumbnailInvalidations(std::unordered_set<std::string>& invalidations)
    {
        if (invalidations.empty() || !Engine::ResourceManager::IsCreated())
            return;

        auto& manager = Engine::ResourceManager::Get();
        std::unordered_map<std::string, std::vector<std::string>> dependents;
        std::unordered_set<std::string> materialKeys;
        std::unordered_set<std::string> fallbackPreviews;

        auto addDependency = [&](const Engine::Resource* dependency, const std::string& ownerKey)
            {
                const auto dependencyKey = ThumbnailResourceKey(dependency);
                if (!dependencyKey.empty() && dependencyKey != ownerKey)
                    dependents[dependencyKey].push_back(ownerKey);
            };

        for (const auto& entry : manager.GetDebugSnapshot())
        {
            auto* resource = manager.GetResource(manager.FindHandleByKey(entry.Key));
            const auto ownerKey = ThumbnailResourceKey(resource);
            if (resource == nullptr || ownerKey.empty())
                continue;

            if (auto* material = dynamic_cast<Engine::MaterialInterface*>(resource))
            {
                materialKeys.insert(ownerKey);

                for (const auto& binding : material->GetTextureBindings())
                    addDependency(binding.texture.Get(), ownerKey);

                if (auto* instance = dynamic_cast<Engine::MaterialInstance*>(material))
                    addDependency(instance->GetBaseMaterial(), ownerKey);
            }
            else if (auto* sprite = dynamic_cast<Engine::Sprite*>(resource))
            {
                addDependency(sprite->GetTexture(), ownerKey);
            }
            else if (auto* model = dynamic_cast<Engine::Model*>(resource))
            {
                const auto meshes = model->GetMeshes();

                for (size_t i = 0; i < meshes.size(); ++i)
                {
                    addDependency(meshes[i].Get(), ownerKey);

                    const auto material = model->GetMeshMaterial(static_cast<Engine::uint32>(i));
                    if (meshes[i].Get() != nullptr && material.Get() == nullptr)
                        fallbackPreviews.insert(ownerKey);
                }

                for (const auto& material : model->GetMaterials())
                    addDependency(material.Get(), ownerKey);
            }
            else if (dynamic_cast<Engine::Mesh*>(resource) != nullptr)
            {
                fallbackPreviews.insert(ownerKey);
            }
        }

        std::vector<std::string> pending(invalidations.begin(), invalidations.end());
        bool materialChanged = false;

        for (size_t i = 0; i < pending.size(); ++i)
        {
            if (materialKeys.contains(pending[i]))
                materialChanged = true;

            const auto found = dependents.find(pending[i]);
            if (found == dependents.end())
                continue;

            for (const auto& ownerKey : found->second)
            {
                if (invalidations.insert(ownerKey).second)
                    pending.push_back(ownerKey);
            }
        }

        // 기본 재질을 사용하는 프리뷰는 재질 변경 시 보수적으로 갱신합니다.
        if (materialChanged)
            invalidations.insert(fallbackPreviews.begin(), fallbackPreviews.end());
    }

    // F12: 고해상도 원본 이미지를 썸네일 해상도로 안전하게 축소하는 Bilinear 다운샘플러
    std::vector<uint8_t> DownsampleRGBA(const uint8_t* src, int32 srcW, int32 srcH, int32 dstW, int32 dstH)
    {
        std::vector<uint8_t> dst(static_cast<size_t>(dstW) * dstH * 4);
        const float xRatio = static_cast<float>(srcW) / static_cast<float>(dstW);
        const float yRatio = static_cast<float>(srcH) / static_cast<float>(dstH);

        for (int32 y = 0; y < dstH; ++y)
        {
            const float srcY = (static_cast<float>(y) + 0.5f) * yRatio - 0.5f;
            const int32 y0 = std::clamp(static_cast<int32>(srcY), 0, srcH - 1);
            const int32 y1 = std::clamp(y0 + 1, 0, srcH - 1);
            const float yWeight = srcY - static_cast<float>(y0);

            for (int32 x = 0; x < dstW; ++x)
            {
                const float srcX = (static_cast<float>(x) + 0.5f) * xRatio - 0.5f;
                const int32 x0 = std::clamp(static_cast<int32>(srcX), 0, srcW - 1);
                const int32 x1 = std::clamp(x0 + 1, 0, srcW - 1);
                const float xWeight = srcX - static_cast<float>(x0);

                const size_t dstIndex = (static_cast<size_t>(y) * dstW + x) * 4;
                const size_t i00 = (static_cast<size_t>(y0) * srcW + x0) * 4;
                const size_t i10 = (static_cast<size_t>(y0) * srcW + x1) * 4;
                const size_t i01 = (static_cast<size_t>(y1) * srcW + x0) * 4;
                const size_t i11 = (static_cast<size_t>(y1) * srcW + x1) * 4;

                for (int32 c = 0; c < 4; ++c)
                {
                    const float c00 = src[i00 + c];
                    const float c10 = src[i10 + c];
                    const float c01 = src[i01 + c];
                    const float c11 = src[i11 + c];
                    const float top = c00 * (1.0f - xWeight) + c10 * xWeight;
                    const float bottom = c01 * (1.0f - xWeight) + c11 * xWeight;
                    const float val = top * (1.0f - yWeight) + bottom * yWeight;
                    dst[dstIndex + c] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
                }
            }
        }
        return dst;
    }
}

IMPLEMENT_SINGLETON(AssetCache)

#pragma region Constructor&Destructor
EResult AssetCache::Initialize(void* arg)
{
    if (!AssetWorkerHost::IsCreated()) (void)AssetWorkerHost::Create();
    if (m_PreRenderHandle == 0 && Engine::Renderer::IsCreated())
    {
        m_PreRenderHandle = Engine::Renderer::Get().GetPreRenderDelegate().AddLambda([this](Engine::f32)
            {
                FlushPendingUploads();
            });
    }
    return EResult::Success;
}

void AssetCache::Free()
{
    if (m_PreRenderHandle != 0 && Engine::Renderer::IsCreated())
    {
        Engine::Renderer::Get().GetPreRenderDelegate().Remove(m_PreRenderHandle);
        m_PreRenderHandle = 0;
    }

    if (AssetWorkerHost::IsCreated()) AssetWorkerHost::Destroy();

    for (auto& worker : m_DecodeWorkers)
    {
        if (worker.Future.valid())
            worker.Future.wait();
    }

    m_DecodeWorkers.clear();

    for (auto* res : m_TempCaptureResources)
    {
        if (res)
        {
            res->Free();
            delete res;
        }
    }
    m_TempCaptureResources.clear();

    ClearAll();
    for (auto& page : m_AtlasPages)
    {
        Safe_Release(page.Texture);
        page.FreeSlots.clear();
    }
    m_AtlasPages.clear();
    PruneDiskCache();
}
#pragma endregion

#pragma region Loop
void AssetCache::Update()
{
    if (AssetWorkerHost::IsCreated())
    {
        for (auto& task : AssetWorkerHost::Get().PollCompletedTasks())
        {
            const auto loading = m_LoadingPaths.find(task.AssetKey);
            if (loading == m_LoadingPaths.end() || loading->second != task.RequestId) continue;

            if (task.Success && task.Width > 0 && task.Height > 0 && !task.RgbaData.empty())
            {
                m_PendingUploadQueue.push_back(ThumbnailUploadTask{
                    task.AssetKey, task.RequestId, task.Width, task.Height, std::move(task.RgbaData) });
            }
            else
            {
                if (!task.OutputPngPath.empty()) SaveNegativeCacheToDisk(task.OutputPngPath);
                m_LoadingPaths.erase(loading);
                InsertToCache(task.AssetKey, nullptr, 0);
            }
        }
    }

    CollectFinishedThumbnailWorkers();

    std::unordered_set<std::string> invalidations;

    {
        std::lock_guard lock(m_UploadMutex);
        invalidations.swap(m_PendingInvalidations);
    }

    ExpandThumbnailInvalidations(invalidations);

    for (const auto& path : invalidations)
        ClearCache(Engine::Utf8ToPath(path));

    if (m_PreRenderHandle == 0)
        FlushPendingUploads();

    // 3D 1프레임 오프스크린 캡처 파이프라인 처리
    ProcessPending3DThumbnail();
}

void AssetCache::FlushPendingUploads()
{
    RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;

    // F12: 프레임당 최대 MAX_UPLOADS_PER_FRAME개씩만 LIFO(가시 영역 우선) 업로드
    size_t uploadsThisFrame = 0;
    while (!m_PendingUploadQueue.empty() && uploadsThisFrame < MAX_UPLOADS_PER_FRAME)
    {
        ThumbnailUploadTask task = std::move(m_PendingUploadQueue.back());
        m_PendingUploadQueue.pop_back();

        const auto loading = m_LoadingPaths.find(task.AssetPath);
        if (loading == m_LoadingPaths.end() || loading->second != task.RequestId)
        {
            BAM_LOG(Trace, "Editor", "Asset",
                "Discarded stale thumbnail: path={}, request={}", task.AssetPath, task.RequestId);
            continue;
        }

        m_LoadingPaths.erase(loading);

        if (rhi && task.Width > 0 && task.Height > 0 && !task.Data.empty() &&
            task.Width <= ATLAS_SLOT_DIM && task.Height <= ATLAS_SLOT_DIM)
        {
            int32 pageIdx = -1;
            int32 slotIdx = -1;
            if (AllocateAtlasSlot(rhi, pageIdx, slotIdx))
            {
                const uint32 dstX = static_cast<uint32>(slotIdx % ATLAS_SLOTS_PER_ROW) * ATLAS_SLOT_DIM;
                const uint32 dstY = static_cast<uint32>(slotIdx / ATLAS_SLOTS_PER_ROW) * ATLAS_SLOT_DIM;

                RHITextureDesc tileDesc = {};
                tileDesc.width = static_cast<uint32>(task.Width);
                tileDesc.height = static_cast<uint32>(task.Height);
                tileDesc.mipLevels = 1;
                tileDesc.arraySize = 1;
                tileDesc.format = Engine::ETextureFormat::R8G8B8A8_UNORM;
                tileDesc.dimension = Engine::ETextureDimension::Texture2D;
                tileDesc.usage = Engine::ETextureUsage::Sampler;
                tileDesc.data = task.Data.data();
                tileDesc.dataSize = static_cast<uint32>(task.Data.size());

                if (Engine::IsSuccess(rhi->UploadTextureData(m_AtlasPages[pageIdx].Texture, tileDesc, dstX, dstY)))
                {
                    const float insetX = task.Width > 2 ? 0.5f : 0.0f;
                    const float insetY = task.Height > 2 ? 0.5f : 0.0f;
                    const ImVec2 uv0((static_cast<float>(dstX) + insetX) / static_cast<float>(ATLAS_PAGE_DIM),
                        (static_cast<float>(dstY) + insetY) / static_cast<float>(ATLAS_PAGE_DIM));
                    const ImVec2 uv1((static_cast<float>(dstX + task.Width) - insetX) / static_cast<float>(ATLAS_PAGE_DIM),
                        (static_cast<float>(dstY + task.Height) - insetY) / static_cast<float>(ATLAS_PAGE_DIM));
                    InsertAtlasToCache(task.AssetPath, m_AtlasPages[pageIdx].Texture, pageIdx, slotIdx,
                        uv0, uv1, task.Width, task.Height, std::move(task.Data));
                    ++uploadsThisFrame;
                    continue;
                }

                FreeAtlasSlot(pageIdx, slotIdx);
            }

            RHITextureDesc desc = {};
            desc.width = static_cast<uint32>(task.Width);
            desc.height = static_cast<uint32>(task.Height);
            desc.data = task.Data.data();
            desc.dataSize = static_cast<uint32>(task.Data.size());

            RHITexture* standalone = task.Data.size() == desc.dataSize ? rhi->CreateTexture(desc) : nullptr;
            if (standalone)
            {
                InsertToCache(task.AssetPath, standalone, task.Data.size());
                ++uploadsThisFrame;
                continue;
            }
        }

        InsertToCache(task.AssetPath, nullptr, 0);
    }
}
#pragma endregion

#pragma region Thumbnail Management
std::string AssetCache::BuildCacheKey(const filesystem::path& assetPath)
{
    return ThumbnailCacheKey(assetPath);
}

AssetCache::ThumbnailView AssetCache::GetThumbnailView(const std::string& cacheKey, const filesystem::path& assetPath)
{
    const std::string key = cacheKey.empty() ? ThumbnailCacheKey(assetPath) : cacheKey;
    if (key.empty())
        return {};

    if (auto cached = m_ThumbnailCache.find(key); cached != m_ThumbnailCache.end())
    {
        m_LruList.splice(m_LruList.begin(), m_LruList, cached->second.LruIt);
        return ThumbnailView{ cached->second.Texture, cached->second.UV0, cached->second.UV1, cached->second.SpriteHandle };
    }

    if (m_LoadingPaths.contains(key))
        return {};

    std::string filename = Engine::WStrToStr(assetPath.filename().wstring());
    for (char& ch : filename)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    std::string ext = Engine::WStrToStr(assetPath.extension().wstring());
    for (char& ch : ext)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    const bool isImage = ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".dds";
    const bool isMaterial = ext == ".bammat" || ext == ".bammatinst" || ext == ".mat" ||
        filename.ends_with(".bammat.json") || filename.ends_with(".bammatinst.json");
    const bool isRawModel = ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb";
    const bool isModel = ext == ".bammodel" || filename.ends_with(".bammodel.json") || isRawModel;
    const bool isMesh = ext == ".bammesh" || filename.ends_with(".bammesh.json");
    const bool isPrefab = IsPrefabThumbnail(filename);
    const bool isTextureOrSprite = ext == ".bamtex" || ext == ".bamsprite" ||
        filename.ends_with(".bamtex.json") || filename.ends_with(".bamsprite.json");

    if (isMaterial || isModel || isMesh || isPrefab || isTextureOrSprite)
    {
        // [신규] 디스크 캐시가 이미 존재하면 오프스크린 렌더링 및 워커 큐를 100% 바이패스하고 비동기 디스크 로드
        const auto diskCachePath = CalculateDiskCachePath(assetPath);
        if (!diskCachePath.empty() && std::filesystem::exists(diskCachePath))
        {
            if (m_DecodeWorkers.size() >= MAX_DECODE_WORKERS)
                return {};
            const Engine::uint64 requestId = ++m_NextRequestId;
            m_LoadingPaths.emplace(key, requestId);
            try
            {
                auto future = std::async(std::launch::async, [diskCachePath, key]()
                    {
                        return AssetCache::LoadThumbnailFromDiskCache(diskCachePath, key);
                    });
                m_DecodeWorkers.push_back(ThumbnailDecodeWorker{ key, requestId, std::move(future) });
            }
            catch (...)
            {
                m_LoadingPaths.erase(key);
                InsertToCache(key, nullptr, 0);
            }
            return {};
        }

        // [방향 B] 3D 모델·메시·프리팹은 독립 워커 프로세스로 100% 격리 베이킹 (크기 제한 없이 무간섭 처리)
        if ((isModel || isMesh || isPrefab) && AssetWorkerHost::IsCreated() && AssetWorkerHost::Get().IsAvailable())
        {
            if (diskCachePath.empty())
            {
                InsertToCache(key, nullptr, 0);
                return {};
            }
            const Engine::uint64 requestId = ++m_NextRequestId;
            if (AssetWorkerHost::Get().EnqueueThumbnail(key, requestId, assetPath, diskCachePath, MAX_THUMBNAIL_DIM))
                m_LoadingPaths.emplace(key, requestId);
            return {};
        }

        // 워커 미사용 시 인-프로세스 폴백: 대용량 모델/메시 Fast Reject로 메인 스레드 프리징 방어
        if (isRawModel || isMesh)
        {
            std::error_code ec;
            const auto fileSize = std::filesystem::file_size(assetPath, ec);
            if (!ec && fileSize > MAX_RAW_MODEL_PARSE_BYTES)
            {
                if (!diskCachePath.empty())
                    SaveNegativeCacheToDisk(diskCachePath);
                InsertToCache(key, nullptr, 0);
                return {};
            }
        }

        if (ext == ".bammodel" || filename.ends_with(".bammodel.json"))
        {
            std::error_code ec;
            const auto fileSize = std::filesystem::file_size(assetPath, ec);
            if (!ec && fileSize > MAX_COMPOSITE_MODEL_PARSE_BYTES)
            {
                if (!diskCachePath.empty())
                    SaveNegativeCacheToDisk(diskCachePath);
                InsertToCache(key, nullptr, 0);
                return {};
            }
        }

        if (m_Pending3DQueue.size() >= MAX_PENDING_3D_REQUESTS)
            return {};
        const Engine::uint64 requestId = ++m_NextRequestId;
        m_LoadingPaths.emplace(key, requestId);
        m_Pending3DQueue.push(Thumbnail3DRequest{ key, requestId });
        return {};
    }

    if (!isImage)
    {
        InsertToCache(key, nullptr, 0);
        return {};
    }

    if (m_DecodeWorkers.size() >= MAX_DECODE_WORKERS)
        return {};

    const Engine::uint64 requestId = ++m_NextRequestId;
    m_LoadingPaths.emplace(key, requestId);

    try
    {
        auto future = std::async(std::launch::async, [key]()
            {
                return AssetCache::LoadImageThumbnail(Engine::Utf8ToPath(key));
            });

        m_DecodeWorkers.push_back(ThumbnailDecodeWorker{ key, requestId, std::move(future) });
    }
    catch (...)
    {
        m_LoadingPaths.erase(key);
        InsertToCache(key, nullptr, 0);
        BAM_LOG(Error, "Editor", "Asset", "Failed to queue thumbnail generation: path={}", key);
    }

    return {};
}

RHITexture* AssetCache::GetThumbnail(const filesystem::path& assetPath, Handle* spriteHandle)
{
    if (spriteHandle) *spriteHandle = {};

    const std::string key = ThumbnailCacheKey(assetPath);
    if (key.empty()) return nullptr;

    (void)GetThumbnailView(key, assetPath);

    auto cached = m_ThumbnailCache.find(key);
    if (cached == m_ThumbnailCache.end()) return nullptr;

    auto& entry = cached->second;
    if (spriteHandle) *spriteHandle = entry.SpriteHandle;

    if (entry.AtlasPage >= 0)
    {
        if (!entry.StandaloneTexture && !entry.RawPixels.empty())
        {
            if (RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr)
            {
                RHITextureDesc desc = {};
                desc.width = static_cast<uint32>(entry.Width);
                desc.height = static_cast<uint32>(entry.Height);
                desc.data = entry.RawPixels.data();
                desc.dataSize = static_cast<uint32>(entry.RawPixels.size());
                entry.StandaloneTexture = rhi->CreateTexture(desc);
            }
        }
        return entry.StandaloneTexture;
    }

    return entry.Texture;
}

void AssetCache::RequestInvalidate(const filesystem::path& assetPath)
{
    const std::string key = ThumbnailCacheKey(assetPath);
    if (key.empty())
        return;

    std::lock_guard lock(m_UploadMutex);
    m_PendingInvalidations.insert(key);
}

void AssetCache::ClearCache(const filesystem::path& assetPath)
{
    const std::string key = ThumbnailCacheKey(assetPath);
    if (key.empty()) return;
    if (auto it = m_ThumbnailCache.find(key); it != m_ThumbnailCache.end())
    {
        m_CurrentCacheBytes -= it->second.ByteSize;
        ReleaseCacheEntryTextures(it->second);
        m_LruList.erase(it->second.LruIt);
        m_ThumbnailCache.erase(it);
    }
    // 디스크 캐시 파일도 함께 삭제
    InvalidateDiskCache(assetPath);
}

void AssetCache::ClearAll()
{
    if (AssetWorkerHost::IsCreated()) AssetWorkerHost::Get().ClearPendingTasks();
    m_LoadingPaths.clear();
    m_Pending3DQueue = {};
    m_CurrentCapturingKey.clear();
    m_CurrentCapturingRequestId = 0;
    m_CaptureState = ECaptureState::Idle;
    for (auto& [path, entry] : m_ThumbnailCache)
        ReleaseCacheEntryTextures(entry);
    m_ThumbnailCache.clear();
    m_LruList.clear();
    m_CurrentCacheBytes = 0;
    m_PendingUploadQueue.clear();

    {
        std::lock_guard lock(m_UploadMutex);
        m_PendingInvalidations.clear();
    }
}

std::filesystem::path AssetCache::GetDiskCacheDirectory()
{
    const auto projectRoot = Engine::PlatformPath::GetProjectRoot();
    return projectRoot / ".bam" / "cache" / "thumbnails";
}

std::filesystem::path AssetCache::CalculateDiskCachePath(const std::filesystem::path& assetPath)
{
    const auto fullPath = Engine::PlatformPath::ResolveProjectRelative(assetPath);
    std::error_code ec;
    const auto lastWrite = std::filesystem::last_write_time(fullPath, ec);
    if (ec) return {};

    const auto fileSize = std::filesystem::file_size(fullPath, ec);
    if (ec) return {};

    const auto relPath = Engine::PlatformPath::ToProjectRelative(fullPath);
    const auto timePoint = lastWrite.time_since_epoch().count();

    auto filename = Engine::WStrToStr(fullPath.filename().wstring());
    for (char& ch : filename)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const char* cacheVersion = IsPrefabThumbnail(filename) ? "_v3" : "_v2";

    // 고유 캐시 키 문자열: 상대경로 + 파일크기 + 수정시각 + 포맷버전
    const std::string seed = Engine::WStrToStr(relPath.generic_wstring()) + "_" +
        std::to_string(fileSize) + "_" + std::to_string(timePoint) + cacheVersion;

    // 64비트 FNV-1a 해시 생성
    uint64_t hash = 14695981039346656037ull;
    for (char c : seed)
    {
        hash ^= static_cast<uint8_t>(c);
        hash *= 1099511628211ull;
    }

    char filenameBuf[32];
    std::snprintf(filenameBuf, sizeof(filenameBuf), "%016llx.png", static_cast<unsigned long long>(hash));

    return GetDiskCacheDirectory() / filenameBuf;
}

std::optional<AssetCache::ThumbnailUploadTask> AssetCache::LoadThumbnailFromDiskCache(const std::filesystem::path& cachePath, const std::string& key)
{
    std::error_code ec;
    if (!std::filesystem::exists(cachePath, ec)) return std::nullopt;

    std::ifstream file(cachePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return std::nullopt;

    const std::streamsize size = file.tellg();
    if (size == 0)
    {
        // 네거티브 캐시 (이전 시도 실패 또는 대용량 스킵): 재시도 없이 기본 아이콘용 빈 태스크 반환
        ThumbnailUploadTask task;
        task.AssetPath = key;
        task.Width = 0;
        task.Height = 0;
        return task;
    }
    if (size < 0 || size > 2 * 1024 * 1024) return std::nullopt; // 캐시 PNG는 수십 KB 이내

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> encodedData(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(encodedData.data()), size)) return std::nullopt;

    int width = 0, height = 0, channels = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(encodedData.data(), static_cast<int>(size), &width, &height, &channels, STBI_rgb_alpha),
        stbi_image_free);

    if (!pixels || width <= 0 || height <= 0 || width > MAX_THUMBNAIL_DIM || height > MAX_THUMBNAIL_DIM)
    {
        std::filesystem::remove(cachePath, ec); // 손상된 캐시 파일은 즉시 삭제
        return std::nullopt;
    }

    ThumbnailUploadTask task;
    task.AssetPath = key;
    task.Width = width;
    task.Height = height;
    task.Data.assign(pixels.get(), pixels.get() + (static_cast<size_t>(width) * height * 4));
    return task;
}

bool AssetCache::SaveThumbnailToDiskCache(const std::filesystem::path& cachePath, int32 width, int32 height, const uint8_t* rgbaData)
{
    if (cachePath.empty() || width <= 0 || height <= 0 || !rgbaData) return false;

    const auto cacheDir = cachePath.parent_path();
    std::error_code ec;
    if (!std::filesystem::exists(cacheDir, ec))
    {
        if (!std::filesystem::create_directories(cacheDir, ec)) return false;
    }

    // 원자적 쓰기: 임시 파일로 먼저 기록 후 rename
    const auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
    const auto tmpPath = cachePath.string() + ".tmp_" + std::to_string(threadId);

    const int stride = width * 4;
    const int result = stbi_write_png(tmpPath.c_str(), width, height, 4, rgbaData, stride);
    if (result == 0)
    {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    std::filesystem::rename(tmpPath, cachePath, ec);
    if (ec)
    {
        std::filesystem::remove(tmpPath, ec);
        return false;
    }

    return true;
}

bool AssetCache::SaveNegativeCacheToDisk(const std::filesystem::path& cachePath)
{
    if (cachePath.empty()) return false;
    const auto cacheDir = cachePath.parent_path();
    std::error_code ec;
    if (!std::filesystem::exists(cacheDir, ec))
    {
        if (!std::filesystem::create_directories(cacheDir, ec)) return false;
    }
    std::ofstream file(cachePath, std::ios::binary | std::ios::trunc);
    return file.is_open();
}

void AssetCache::InvalidateDiskCache(const std::filesystem::path& assetPath)
{
    const auto cachePath = CalculateDiskCachePath(assetPath);
    if (cachePath.empty()) return;

    std::error_code ec;
    std::filesystem::remove(cachePath, ec);
}

void AssetCache::PruneDiskCache(size_t maxBytes)
{
    const auto cacheDir = GetDiskCacheDirectory();
    std::error_code ec;
    if (!std::filesystem::exists(cacheDir, ec)) return;
    struct CacheFileInfo
    {
        std::filesystem::path Path;
        std::filesystem::file_time_type LastWriteTime;
        uintmax_t Size = 0;
    };
    std::vector<CacheFileInfo> files;
    uintmax_t totalBytes = 0;
    for (const auto& entry : std::filesystem::directory_iterator(cacheDir, ec))
    {
        if (entry.is_regular_file(ec) && entry.path().extension() == ".png")
        {
            const auto size = entry.file_size(ec);
            const auto lwt = entry.last_write_time(ec);
            if (!ec)
            {
                files.push_back({ entry.path(), lwt, size });
                totalBytes += size;
            }
        }
    }
    if (totalBytes <= maxBytes) return;
    // 가장 오래된 파일이 앞에 오도록 정렬
    std::sort(files.begin(), files.end(), [](const CacheFileInfo& a, const CacheFileInfo& b)
        {
            return a.LastWriteTime < b.LastWriteTime;
        });
    const uintmax_t targetBytes = static_cast<uintmax_t>(maxBytes * 0.8f); // 80% 수준까지 정리
    for (const auto& file : files)
    {
        if (totalBytes <= targetBytes) break;
        if (std::filesystem::remove(file.Path, ec))
        {
            totalBytes -= file.Size;
        }
    }
}

std::optional<AssetCache::ThumbnailUploadTask> AssetCache::LoadImageThumbnail(const filesystem::path& assetPath)
{
    const auto fullPath = Engine::PlatformPath::ResolveProjectRelative(assetPath);
    const auto key = ThumbnailCacheKey(fullPath);
    // 1. 디스크 캐시 우선 확인 (캐시 적중 시 원본 수십 MB 로드/디코딩 100% 스킵!)
    const auto diskCachePath = CalculateDiskCachePath(fullPath);
    if (!diskCachePath.empty())
    {
        auto cachedTask = LoadThumbnailFromDiskCache(diskCachePath, key);
        if (cachedTask.has_value())
            return cachedTask;
    }
    // 2. 디스크 캐시 미적중 시: 원본 파일 로드 및 다운샘플링 (기존 로직)
    std::string ext = fullPath.extension().string();
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (ext == ".dds")
    {
        DirectX::ScratchImage ddsImage;
        HRESULT hr = DirectX::LoadFromDDSFile(fullPath.wstring().c_str(), DirectX::DDS_FLAGS_NONE, nullptr, ddsImage);
        if (FAILED(hr)) return std::nullopt;

        const DirectX::ScratchImage* finalImage = &ddsImage;
        DirectX::ScratchImage decompressedImage;
        if (DirectX::IsCompressed(ddsImage.GetMetadata().format))
        {
            hr = DirectX::Decompress(*ddsImage.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, decompressedImage);
            if (FAILED(hr)) return std::nullopt;
            finalImage = &decompressedImage;
        }
        else if (ddsImage.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
        {
            hr = DirectX::Convert(*ddsImage.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, 0.5f, decompressedImage);
            if (FAILED(hr)) return std::nullopt;
            finalImage = &decompressedImage;
        }

        const auto* img = finalImage->GetImage(0, 0, 0);
        if (!img || !img->pixels) return std::nullopt;

        const int width = static_cast<int>(img->width);
        const int height = static_cast<int>(img->height);
        if (width <= 0 || height <= 0) return std::nullopt;

        ThumbnailUploadTask task;
        task.AssetPath = key;

        if (width > MAX_THUMBNAIL_DIM || height > MAX_THUMBNAIL_DIM)
        {
            const float aspect = static_cast<float>(width) / static_cast<float>(height);
            int32 targetW = MAX_THUMBNAIL_DIM;
            int32 targetH = MAX_THUMBNAIL_DIM;
            if (aspect >= 1.0f)
                targetH = std::max(1, static_cast<int32>(MAX_THUMBNAIL_DIM / aspect));
            else
                targetW = std::max(1, static_cast<int32>(MAX_THUMBNAIL_DIM * aspect));
            task.Width = targetW;
            task.Height = targetH;
            task.Data = DownsampleRGBA(img->pixels, width, height, targetW, targetH);
        }
        else
        {
            task.Width = width;
            task.Height = height;
            task.Data.assign(img->pixels, img->pixels + (static_cast<size_t>(width) * height * 4));
        }

        if (!diskCachePath.empty() && !task.Data.empty())
        {
            SaveThumbnailToDiskCache(diskCachePath, task.Width, task.Height, task.Data.data());
        }
        return task;
    }

    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return std::nullopt;
    const std::streamsize size = file.tellg();
    if (size <= 0) return std::nullopt;
    if (size > static_cast<std::streamsize>(MAX_THUMBNAIL_SOURCE_BYTES) ||
        size > static_cast<std::streamsize>((std::numeric_limits<int>::max)()))
    {
        BAM_LOG(Warn, "Editor", "Asset",
            "Thumbnail source exceeds file size limit: path={}, bytes={}", key, size);
        return std::nullopt;
    }
    file.seekg(0, std::ios::beg);
    std::vector<stbi_uc> encodedData(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(encodedData.data()), size))
        return std::nullopt;
    const int encodedSize = static_cast<int>(size);
    int width = 0, height = 0, channels = 0;
    if (!stbi_info_from_memory(encodedData.data(), encodedSize, &width, &height, &channels))
        return std::nullopt;
    if (width <= 0 || height <= 0)
        return std::nullopt;
    if (width > MAX_THUMBNAIL_SOURCE_DIM || height > MAX_THUMBNAIL_SOURCE_DIM)
    {
        BAM_LOG(Warn, "Editor", "Asset",
            "Thumbnail source exceeds dimension limit: path={}, size={}x{}", key, width, height);
        return std::nullopt;
    }
    const Engine::uint64 rgbaBytes = static_cast<Engine::uint64>(width) * height * STBI_rgb_alpha;
    if (rgbaBytes > MAX_THUMBNAIL_RGBA_BYTES)
    {
        BAM_LOG(Warn, "Editor", "Asset",
            "Thumbnail source exceeds RGBA size limit: path={}, bytes={}", key, rgbaBytes);
        return std::nullopt;
    }
    int decodedWidth = 0, decodedHeight = 0;
    std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(encodedData.data(), encodedSize, &decodedWidth, &decodedHeight,
            &channels, STBI_rgb_alpha),
        stbi_image_free);
    if (!pixels || decodedWidth != width || decodedHeight != height)
        return std::nullopt;
    ThumbnailUploadTask task;
    task.AssetPath = key;
    if (width > MAX_THUMBNAIL_DIM || height > MAX_THUMBNAIL_DIM)
    {
        const float aspect = static_cast<float>(width) / static_cast<float>(height);
        int32 targetW = MAX_THUMBNAIL_DIM;
        int32 targetH = MAX_THUMBNAIL_DIM;
        if (aspect >= 1.0f)
            targetH = std::max(1, static_cast<int32>(MAX_THUMBNAIL_DIM / aspect));
        else
            targetW = std::max(1, static_cast<int32>(MAX_THUMBNAIL_DIM * aspect));
        task.Width = targetW;
        task.Height = targetH;
        task.Data = DownsampleRGBA(pixels.get(), width, height, targetW, targetH);
    }
    else
    {
        task.Width = width;
        task.Height = height;
        task.Data.assign(pixels.get(), pixels.get() + static_cast<size_t>(rgbaBytes));
    }
    // 3. 축소 완료된 128x128 RGBA 데이터를 디스크 캐시 파일로 영속화
    if (!diskCachePath.empty() && !task.Data.empty())
    {
        SaveThumbnailToDiskCache(diskCachePath, task.Width, task.Height, task.Data.data());
    }
    return task;
}

void AssetCache::CollectFinishedThumbnailWorkers()
{
    for (auto it = m_DecodeWorkers.begin(); it != m_DecodeWorkers.end();)
    {
        if (it->Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
        {
            ++it;
            continue;
        }

        const auto loading = m_LoadingPaths.find(it->AssetPath);
        if (loading == m_LoadingPaths.end() || loading->second != it->RequestId)
        {
            it = m_DecodeWorkers.erase(it);
            continue;
        }

        ThumbnailUploadTask task;

        try
        {
            auto decoded = it->Future.get();
            if (decoded)
                task = std::move(*decoded);
        }
        catch (const std::exception& error)
        {
            BAM_LOG(Error, "Editor", "Asset",
                "Thumbnail decode failed: path={}, error={}", it->AssetPath, error.what());
        }
        catch (...)
        {
            BAM_LOG(Error, "Editor", "Asset",
                "Thumbnail decode threw an unknown exception: path={}", it->AssetPath);
        }

        task.AssetPath = it->AssetPath;
        task.RequestId = it->RequestId;
        m_PendingUploadQueue.push_back(std::move(task));

        it = m_DecodeWorkers.erase(it);
    }
}

std::optional<AssetCache::ThumbnailUploadTask> AssetCache::LoadModelThumbnail(const filesystem::path& assetPath)
{
    HRESULT hr = CoInitialize(NULL);
    IShellItemImageFactory* imageFactory = nullptr;
    hr = SHCreateItemFromParsingName(std::filesystem::absolute(assetPath).wstring().c_str(), nullptr, IID_PPV_ARGS(&imageFactory));

    if (SUCCEEDED(hr) && imageFactory)
    {
        HBITMAP hBitmap;
        SIZE size = { MAX_THUMBNAIL_DIM, MAX_THUMBNAIL_DIM };
        hr = imageFactory->GetImage(size, SIIGBF_RESIZETOFIT, &hBitmap);
        imageFactory->Release();

        if (SUCCEEDED(hr))
        {
            BITMAP bm;
            GetObject(hBitmap, sizeof(bm), &bm);
            int width = bm.bmWidth;
            int height = bm.bmHeight;

            BITMAPINFO bi = { 0 };
            bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bi.bmiHeader.biWidth = width;
            bi.bmiHeader.biHeight = -height;
            bi.bmiHeader.biPlanes = 1;
            bi.bmiHeader.biBitCount = 32;
            bi.bmiHeader.biCompression = BI_RGB;

            std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
            HDC hdc = GetDC(NULL);
            GetDIBits(hdc, hBitmap, 0, height, pixels.data(), &bi, DIB_RGB_COLORS);
            ReleaseDC(NULL, hdc);
            DeleteObject(hBitmap);

            // BGRA -> RGBA 스왑 및 알파 보정
            for (size_t i = 0; i < pixels.size(); i += 4)
            {
                std::swap(pixels[i], pixels[i + 2]);
                pixels[i + 3] = 255;
            }

            ThumbnailUploadTask task;
            task.AssetPath = ThumbnailCacheKey(assetPath);
            task.Width = width;
            task.Height = height;
            task.Data = std::move(pixels);

            CoUninitialize();
            return task;
        }
    }
    CoUninitialize();
    return std::nullopt;
}

bool AssetCache::AllocateAtlasSlot(RHI* rhi, int32& outPage, int32& outSlot)
{
    for (size_t p = 0; p < m_AtlasPages.size(); ++p)
    {
        if (!m_AtlasPages[p].FreeSlots.empty() && m_AtlasPages[p].Texture)
        {
            outPage = static_cast<int32>(p);
            outSlot = m_AtlasPages[p].FreeSlots.back();
            m_AtlasPages[p].FreeSlots.pop_back();
            return true;
        }
    }

    if (!rhi) return false;

    RHITextureDesc desc{};
    desc.width = ATLAS_PAGE_DIM;
    desc.height = ATLAS_PAGE_DIM;
    desc.mipLevels = 1;
    desc.arraySize = 1;
    desc.format = Engine::ETextureFormat::R8G8B8A8_UNORM;
    desc.dimension = Engine::ETextureDimension::Texture2D;
    desc.sampleCount = Engine::ETextureSampleCount::TextureSampleCount1;
    desc.usage = Engine::ETextureUsage::Sampler;
    desc.data = nullptr;
    desc.dataSize = 0;

    RHITexture* pageTex = rhi->CreateTexture(desc);
    if (!pageTex) return false;

    AtlasPage newPage;
    newPage.Texture = pageTex;
    newPage.FreeSlots.reserve(ATLAS_SLOTS_PER_PAGE);
    for (int32 s = ATLAS_SLOTS_PER_PAGE - 1; s >= 1; --s)
        newPage.FreeSlots.push_back(s);

    outPage = static_cast<int32>(m_AtlasPages.size());
    outSlot = 0;
    m_AtlasPages.push_back(std::move(newPage));
    return true;
}

void AssetCache::FreeAtlasSlot(int32 page, int32 slot)
{
    if (page < 0 || static_cast<size_t>(page) >= m_AtlasPages.size()) return;
    if (slot < 0 || slot >= ATLAS_SLOTS_PER_PAGE) return;
    m_AtlasPages[page].FreeSlots.push_back(slot);
}

void AssetCache::ReleaseCacheEntryTextures(CacheEntry& entry)
{
    if (entry.AtlasPage >= 0)
    {
        FreeAtlasSlot(entry.AtlasPage, entry.AtlasSlot);
        entry.Texture = nullptr;
        entry.AtlasPage = -1;
        entry.AtlasSlot = -1;
    }
    else
    {
        Safe_Release(entry.Texture);
    }
    Safe_Release(entry.StandaloneTexture);
    entry.RawPixels.clear();
}

void AssetCache::InsertToCache(const std::string& key, RHITexture* texture, size_t byteSize, Engine::Handle spriteHandle)
{
    // 이미 캐시되어 있던 항목이면 이전 리소스 정리
    if (auto it = m_ThumbnailCache.find(key); it != m_ThumbnailCache.end())
    {
        m_CurrentCacheBytes -= it->second.ByteSize;
        ReleaseCacheEntryTextures(it->second);
        m_LruList.erase(it->second.LruIt);
        m_ThumbnailCache.erase(it);
    }

    // F12: 캐시 항목 수 또는 VRAM 바이트 예산 초과 시 LRU 순으로 오래된 텍스처 퇴거
    while (!m_LruList.empty() &&
        (m_ThumbnailCache.size() >= MAX_CACHE_ENTRIES ||
            (texture && m_CurrentCacheBytes + byteSize > MAX_CACHE_BYTES)))
    {
        EvictOldest();
    }

    m_LruList.push_front(key);
    CacheEntry entry;
    entry.Texture = texture;
    entry.ByteSize = texture ? byteSize : 0;
    entry.LruIt = m_LruList.begin();
    entry.SpriteHandle = spriteHandle;

    m_CurrentCacheBytes += entry.ByteSize;
    m_ThumbnailCache.emplace(key, std::move(entry));
}

void AssetCache::InsertAtlasToCache(const std::string& key, RHITexture* atlasTexture, int32 page, int32 slot,
    const ImVec2& uv0, const ImVec2& uv1, int32 width, int32 height, std::vector<uint8_t>&& rawPixels)
{
    const size_t byteSize = static_cast<size_t>(width) * height * 4;
    if (auto it = m_ThumbnailCache.find(key); it != m_ThumbnailCache.end())
    {
        m_CurrentCacheBytes -= it->second.ByteSize;
        ReleaseCacheEntryTextures(it->second);
        m_LruList.erase(it->second.LruIt);
        m_ThumbnailCache.erase(it);
    }

    while (!m_LruList.empty() &&
        (m_ThumbnailCache.size() >= MAX_CACHE_ENTRIES || m_CurrentCacheBytes + byteSize > MAX_CACHE_BYTES))
    {
        EvictOldest();
    }

    m_LruList.push_front(key);
    CacheEntry entry;
    entry.Texture = atlasTexture;
    entry.UV0 = uv0;
    entry.UV1 = uv1;
    entry.AtlasPage = page;
    entry.AtlasSlot = slot;
    entry.Width = width;
    entry.Height = height;
    entry.RawPixels = std::move(rawPixels);
    entry.ByteSize = byteSize;
    entry.LruIt = m_LruList.begin();

    m_CurrentCacheBytes += entry.ByteSize;
    m_ThumbnailCache.emplace(key, std::move(entry));
}

void AssetCache::EvictOldest()
{
    if (m_LruList.empty()) return;

    const std::string oldestKey = m_LruList.back();
    m_LruList.pop_back();

    if (auto it = m_ThumbnailCache.find(oldestKey); it != m_ThumbnailCache.end())
    {
        m_CurrentCacheBytes -= it->second.ByteSize;
        ReleaseCacheEntryTextures(it->second);
        m_ThumbnailCache.erase(it);
    }
}

void AssetCache::ProcessPending3DThumbnail()
{
    auto* thumbRenderer = ThumbnailRenderer::IsCreated() ? &ThumbnailRenderer::Get() : nullptr;
    auto* rhi = Engine::Renderer::IsCreated() ? Engine::Renderer::Get().GetRHI() : nullptr;

    // 1단계: 이전 프레임에서 제출한 렌더링 결과물이 있으면 아틀라스 업로드 큐 및 디스크 캐시로 전달
    if (m_CaptureState == ECaptureState::Rendering)
    {
        const auto loading = m_LoadingPaths.find(m_CurrentCapturingKey);
        const bool isCurrent = thumbRenderer != nullptr && loading != m_LoadingPaths.end() &&
            loading->second == m_CurrentCapturingRequestId;

        if (isCurrent)
        {
            RHITexture* newTexture = thumbRenderer->CreateThumbnailTexture();
            bool queuedToAtlas = false;

            if (newTexture != nullptr && rhi != nullptr)
            {
                std::vector<uint8_t> pixels(MAX_THUMBNAIL_DIM * MAX_THUMBNAIL_DIM * 4);
                if (Engine::IsSuccess(rhi->ReadTexturePixels(newTexture, pixels.data(), static_cast<uint32>(pixels.size()))))
                {
                    const auto diskCachePath = CalculateDiskCachePath(Engine::Utf8ToPath(m_CurrentCapturingKey));
                    if (!diskCachePath.empty())
                    {
                        std::async(std::launch::async, [diskCachePath, pixelsCopy = pixels]()
                            {
                                AssetCache::SaveThumbnailToDiskCache(diskCachePath, MAX_THUMBNAIL_DIM, MAX_THUMBNAIL_DIM, pixelsCopy.data());
                            });
                    }
                    Safe_Release(newTexture);
                    m_PendingUploadQueue.push_back(ThumbnailUploadTask{
                        m_CurrentCapturingKey, m_CurrentCapturingRequestId, MAX_THUMBNAIL_DIM, MAX_THUMBNAIL_DIM, std::move(pixels) });
                    queuedToAtlas = true;
                }
            }

            if (!queuedToAtlas)
            {
                m_LoadingPaths.erase(loading);
                const size_t byteSize = newTexture != nullptr ?
                    static_cast<size_t>(MAX_THUMBNAIL_DIM) * MAX_THUMBNAIL_DIM * 4 : 0;
                InsertToCache(m_CurrentCapturingKey, newTexture, byteSize);
            }
        }
        else
        {
            BAM_LOG(Trace, "Editor", "Asset",
                "Discarded stale 3D thumbnail: path={}, request={}",
                m_CurrentCapturingKey, m_CurrentCapturingRequestId);
        }

        m_CurrentCapturingKey.clear();
        m_CurrentCapturingRequestId = 0;
        m_CaptureState = ECaptureState::Idle;
        for (auto* res : m_TempCaptureResources)
        {
            if (res)
            {
                res->Free();
                delete res;
            }
        }
        m_TempCaptureResources.clear();
    }

    // 2단계: 대기열에서 다음 에셋 1개를 꺼내어 타입별 자동 프레이밍 렌더링 제출
    for (size_t processed = 0;
        processed < MAX_3D_REQUESTS_PER_FRAME && !m_Pending3DQueue.empty() &&
        m_CaptureState == ECaptureState::Idle;
        ++processed)
    {
        const auto request = std::move(m_Pending3DQueue.front());
        m_Pending3DQueue.pop();

        const auto& key = request.AssetPath;
        const auto loading = m_LoadingPaths.find(key);

        if (loading == m_LoadingPaths.end() || loading->second != request.RequestId ||
            m_ThumbnailCache.contains(key))
            continue;

        const filesystem::path path = Engine::Utf8ToPath(key);
        std::string ext = Engine::WStrToStr(path.extension().wstring());
        for (char& c : ext)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string filename = Engine::WStrToStr(path.filename().wstring());
        for (char& c : filename)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        const std::wstring relPath = Engine::PlatformPath::ToVirtualUri(path);

        auto& rm = Engine::ResourceManager::Get();
        bool submitSuccess = false;

        const bool isMat = ext == ".bammat" || ext == ".bammatinst" || ext == ".mat" ||
            filename.ends_with(".bammat.json") || filename.ends_with(".bammatinst.json");
        const bool isRawModel = ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb";
        const bool isModel = ext == ".bammodel" || filename.ends_with(".bammodel.json") || isRawModel;
        const bool isMesh = ext == ".bammesh" || filename.ends_with(".bammesh.json");
        const bool isPrefab = IsPrefabThumbnail(filename);
        const bool isTex = ext == ".bamtex" || filename.ends_with(".bamtex.json");
        const bool isSprite = ext == ".bamsprite" || filename.ends_with(".bamsprite.json");

        if ((isMat || isModel || isMesh || isPrefab) && thumbRenderer == nullptr)
        {
            m_LoadingPaths.erase(key);
            InsertToCache(key, nullptr, 0);
            continue;
        }

        if (isTex)
        {
            auto handle = rm.GetResourceHandle<Engine::Texture>(relPath);
            if (!handle.IsValid())
            {
                auto loaded = rm.LoadFile(relPath);
                if (loaded.IsValid()) handle = Engine::ResourceHandle<Engine::Texture>(loaded);
            }
            if (handle.IsValid() && handle.Get())
            {
                if (auto* rhiTex = handle.Get()->GetRHITexture())
                {
                    Engine::Safe_AddRef(rhiTex);
                    InsertToCache(key, rhiTex, 0);
                    m_LoadingPaths.erase(key);
                    continue;
                }
            }
            const auto diskCachePath = CalculateDiskCachePath(path);
            if (!diskCachePath.empty())
            {
                SaveNegativeCacheToDisk(diskCachePath);
            }
            InsertToCache(key, nullptr, 0);
            m_LoadingPaths.erase(key);
            continue;
        }
        else if (isSprite)
        {
            auto handle = rm.GetResourceHandle<Engine::Sprite>(relPath);
            if (!handle.IsValid())
            {
                auto loaded = rm.LoadFile(relPath);
                if (loaded.IsValid()) handle = Engine::ResourceHandle<Engine::Sprite>(loaded);
            }
            if (handle.IsValid() && handle.Get())
            {
                if (auto* tex = handle.Get()->GetTexture())
                {
                    if (auto* rhiTex = tex->GetRHITexture())
                    {
                        Engine::Safe_AddRef(rhiTex);
                        InsertToCache(key, rhiTex, 0, handle.GetRawHandle());
                        m_LoadingPaths.erase(key);
                        continue;
                    }
                }
            }
            const auto diskCachePath = CalculateDiskCachePath(path);
            if (!diskCachePath.empty())
            {
                SaveNegativeCacheToDisk(diskCachePath);
            }
            InsertToCache(key, nullptr, 0);
            m_LoadingPaths.erase(key);
            continue;
        }

        if (isMat)
        {
            auto handle = rm.GetResourceHandle<Engine::MaterialInterface>(relPath);
            if (!handle.IsValid())
            {
                auto loaded = rm.LoadFile(relPath);
                if (loaded.IsValid()) handle = Engine::ResourceHandle<Engine::MaterialInterface>(loaded);
            }
            if (handle.IsValid() && handle.Get()) submitSuccess = thumbRenderer->SubmitMaterial(handle.Get());
        }
        else if (isModel)
        {
            if (isRawModel)
            {
                // 1. 동명의 임포트된 .bammodel이 존재하는지 먼저 탐색
                const auto parentDir = path.parent_path();
                const auto stem = path.stem().wstring();
                const auto candidate1 = parentDir / (stem + L".bammodel");
                const auto candidate2 = parentDir / stem / (stem + L".bammodel");

                std::filesystem::path modelToLoad;
                std::error_code ec;
                if (std::filesystem::exists(candidate1, ec)) modelToLoad = candidate1;
                else if (std::filesystem::exists(candidate2, ec)) modelToLoad = candidate2;

                if (!modelToLoad.empty())
                {
                    const auto modelFileSize = std::filesystem::file_size(modelToLoad, ec);
                    if (!ec && modelFileSize <= MAX_COMPOSITE_MODEL_PARSE_BYTES)
                    {
                        const std::wstring modelRel = Engine::PlatformPath::ToVirtualUri(modelToLoad);
                        auto handle = rm.GetResourceHandle<Engine::Model>(modelRel);
                        if (!handle.IsValid())
                        {
                            auto loaded = rm.LoadFile(modelRel);
                            if (loaded.IsValid()) handle = Engine::ResourceHandle<Engine::Model>(loaded);
                        }
                        if (handle.IsValid() && handle.Get()) submitSuccess = thumbRenderer->SubmitModel(handle.Get());
                    }
                    else
                    {
                        BAM_LOG(Info, "Editor", "Asset", "Skipped linked live 3D thumbnail for large composite model (>64KB): {}", key);
                    }
                }
                else
                {
                    const auto fileSize = std::filesystem::file_size(path, ec);
                    if (!ec && fileSize <= MAX_RAW_MODEL_PARSE_BYTES)
                    {
                        // 2. 4MB 이하의 원본 모델만 ModelConverter를 통해 가볍게 로드하여 임시 Mesh 렌더링
                        Tools::ModelConversion::Options opt{};
                        opt.FlipUVs = true;
                        const auto result = Tools::ModelConversion::ConvertStaticModel(path, opt);
                        if (result && result.Model.has_value() && !result.Model->Meshes.empty())
                        {
                            const auto& srcMesh = result.Model->Meshes[0];
                            if (!srcMesh.Vertices.empty() && !srcMesh.Indices.empty())
                            {
                                Engine::MeshCreateDesc meshDesc{};
                                const auto vertexCount = static_cast<Engine::uint32>(srcMesh.Vertices.size());

                                std::vector<Engine::VertexPosition> pos(vertexCount);
                                std::vector<Engine::VertexMaterial> attr(vertexCount);
                                for (size_t vIdx = 0; vIdx < vertexCount; ++vIdx)
                                {
                                    pos[vIdx].position = glm::vec3(srcMesh.Vertices[vIdx].Position[0], srcMesh.Vertices[vIdx].Position[1], srcMesh.Vertices[vIdx].Position[2]);
                                    attr[vIdx].normal = glm::vec3(srcMesh.Vertices[vIdx].Normal[0], srcMesh.Vertices[vIdx].Normal[1], srcMesh.Vertices[vIdx].Normal[2]);
                                    attr[vIdx].texCoord = glm::vec2(srcMesh.Vertices[vIdx].TexCoord[0], srcMesh.Vertices[vIdx].TexCoord[1]);
                                    attr[vIdx].texCoord1 = glm::vec2(srcMesh.Vertices[vIdx].TexCoord1[0], srcMesh.Vertices[vIdx].TexCoord1[1]);
                                    attr[vIdx].tangent = glm::vec3(srcMesh.Vertices[vIdx].Tangent[0], srcMesh.Vertices[vIdx].Tangent[1], srcMesh.Vertices[vIdx].Tangent[2]);
                                    attr[vIdx].binormal = glm::vec3(srcMesh.Vertices[vIdx].Bitangent[0], srcMesh.Vertices[vIdx].Bitangent[1], srcMesh.Vertices[vIdx].Bitangent[2]);
                                }

                                meshDesc.Streams[static_cast<Engine::uint32>(Engine::EMeshStream::Position)] = { pos.data(), vertexCount, sizeof(Engine::VertexPosition) };
                                meshDesc.Streams[static_cast<Engine::uint32>(Engine::EMeshStream::Material)] = { attr.data(), vertexCount, sizeof(Engine::VertexMaterial) };
                                meshDesc.IndexData = const_cast<Engine::uint32*>(srcMesh.Indices.data());
                                meshDesc.IndexCount = static_cast<Engine::uint32>(srcMesh.Indices.size());
                                meshDesc.IndexStride = sizeof(Engine::uint32);
                                meshDesc.BoundingBoxMin = glm::vec3(srcMesh.BoundsMin[0], srcMesh.BoundsMin[1], srcMesh.BoundsMin[2]);
                                meshDesc.BoundingBoxMax = glm::vec3(srcMesh.BoundsMax[0], srcMesh.BoundsMax[1], srcMesh.BoundsMax[2]);

                                auto* tempMesh = Engine::Mesh::Create(&meshDesc);
                                if (tempMesh)
                                {
                                    submitSuccess = thumbRenderer->SubmitMesh(tempMesh);
                                    m_TempCaptureResources.push_back(tempMesh);
                                }
                            }
                        }
                    }
                    else
                    {
                        BAM_LOG(Info, "Editor", "Asset", "Skipped live 3D thumbnail for large raw model (>4MB): {}", key);
                    }
                }
            }
            else
            {
                std::error_code ec;
                const auto fileSize = std::filesystem::file_size(path, ec);
                if (!ec && fileSize <= MAX_COMPOSITE_MODEL_PARSE_BYTES)
                {
                    auto handle = rm.GetResourceHandle<Engine::Model>(relPath);
                    if (!handle.IsValid())
                    {
                        auto loaded = rm.LoadFile(relPath);
                        if (loaded.IsValid()) handle = Engine::ResourceHandle<Engine::Model>(loaded);
                    }
                    if (handle.IsValid() && handle.Get()) submitSuccess = thumbRenderer->SubmitModel(handle.Get());
                }
                else
                {
                    BAM_LOG(Info, "Editor", "Asset", "Skipped live 3D thumbnail for large composite model (>64KB): {}", key);
                }
            }
        }
        else if (isPrefab)
        {
            submitSuccess = thumbRenderer->SubmitPrefab(path);
        }
        else if (isMesh)
        {
            std::error_code ec;
            const auto fileSize = std::filesystem::file_size(path, ec);
            if (!ec && fileSize <= MAX_RAW_MODEL_PARSE_BYTES)
            {
                auto handle = rm.GetResourceHandle<Engine::Mesh>(relPath);
                if (!handle.IsValid())
                {
                    auto loaded = rm.LoadFile(relPath);
                    if (loaded.IsValid()) handle = Engine::ResourceHandle<Engine::Mesh>(loaded);
                }
                if (handle.IsValid() && handle.Get()) submitSuccess = thumbRenderer->SubmitMesh(handle.Get());
            }
        }

        if (submitSuccess)
        {
            m_CurrentCapturingKey = key;
            m_CurrentCapturingRequestId = request.RequestId;
            m_CaptureState = ECaptureState::Rendering;
            break;
        }
        else
        {
            const auto diskCachePath = CalculateDiskCachePath(path);
            if (!diskCachePath.empty())
            {
                SaveNegativeCacheToDisk(diskCachePath);
            }
            m_LoadingPaths.erase(key);
            InsertToCache(key, nullptr, 0);
        }
    }
}
#pragma endregion
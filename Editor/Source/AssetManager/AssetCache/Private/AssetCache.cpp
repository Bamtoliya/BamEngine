#pragma once
#include "AssetCache.h"
#include <stb_image.h>
#include "Renderer.h"
#include "RHI.h"
#include <shlobj.h> // Windows Shell API
#include "AssetManager.h"
#include "RHITexture.h"
#include "ThumbnailRenderer.h"
#include "ResourceManager.h"
#include "MaterialInterface.h"
#include "Texture.h"
#include "Sprite.h"
#include "Model.h"
#include "Mesh.h"
#include "Functions.h"
#include "PlatformPath.h"
#include <fstream>
#include <limits>

namespace
{
    std::string ThumbnailCacheKey(const filesystem::path& path)
    {
        if (path.empty()) return {};

        const auto absolutePath = Engine::PlatformPath::ResolveProjectRelative(path);
        return Engine::WStrToStr(absolutePath.generic_wstring());
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
    return EResult::Success;
}

void AssetCache::Free()
{
    ClearAll();
}

void AssetCache::Update()
{
    std::vector<ThumbnailUploadTask> tasks;
    std::unordered_set<std::string> invalidations;

    {
        std::lock_guard lock(m_UploadMutex);
        tasks.swap(m_UploadTasks);
        invalidations.swap(m_PendingInvalidations);
    }

    // 같은 프레임에 변경 요청과 완료 결과가 들어오면 변경부터 반영합니다.
    for (const auto& path : invalidations)
        ClearCache(Engine::Utf8ToPath(path));

    // 새로 도착한 태스크들을 메인 스레드 업로드 대기열에 추가
    for (auto& task : tasks)
        m_PendingUploadQueue.push_back(std::move(task));

    RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;

    // F12: 프레임당 최대 MAX_UPLOADS_PER_FRAME개씩만 텍스처를 생성하여 프레임 드랍을 방지합니다.
    size_t uploadsThisFrame = 0;
    while (!m_PendingUploadQueue.empty() && uploadsThisFrame < MAX_UPLOADS_PER_FRAME)
    {
        ThumbnailUploadTask task = std::move(m_PendingUploadQueue.front());
        m_PendingUploadQueue.pop_front();

        const auto loading = m_LoadingPaths.find(task.AssetPath);
        if (loading == m_LoadingPaths.end() || loading->second != task.RequestId)
        {
            BAM_LOG(Trace, "Editor", "Asset",
                "Discarded stale thumbnail: path={}, request={}", task.AssetPath, task.RequestId);
            continue;
        }

        m_LoadingPaths.erase(loading);

        RHITexture* texture = nullptr;
        size_t byteSize = 0;

        if (rhi && task.Width > 0 && task.Height > 0 && !task.Data.empty())
        {
            RHITextureDesc desc = {};
            desc.width = task.Width;
            desc.height = task.Height;
            desc.data = task.Data.data();
            desc.dataSize = static_cast<uint32>(task.Data.size());

            if (task.Data.size() == desc.dataSize)
            {
                texture = rhi->CreateTexture(desc);
                if (texture)
                {
                    byteSize = task.Data.size();
                    ++uploadsThisFrame;
                }
            }
        }

        InsertToCache(task.AssetPath, texture, byteSize);
    }

    // 3D 1프레임 오프스크린 캡처 파이프라인 처리
    ProcessPending3DThumbnail();
}

void AssetCache::InsertToCache(const std::string& key, RHITexture* texture, size_t byteSize)
{
    // 이미 캐시되어 있던 항목이면 이전 리소스 정리
    if (auto it = m_ThumbnailCache.find(key); it != m_ThumbnailCache.end())
    {
        m_CurrentCacheBytes -= it->second.ByteSize;
        Safe_Release(it->second.Texture);
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

    m_CurrentCacheBytes += entry.ByteSize;
    m_ThumbnailCache.emplace(key, entry);
}

void AssetCache::EvictOldest()
{
    if (m_LruList.empty()) return;

    const std::string oldestKey = m_LruList.back();
    m_LruList.pop_back();

    if (auto it = m_ThumbnailCache.find(oldestKey); it != m_ThumbnailCache.end())
    {
        m_CurrentCacheBytes -= it->second.ByteSize;
        Safe_Release(it->second.Texture);
        m_ThumbnailCache.erase(it);
    }
}

void AssetCache::ProcessPending3DThumbnail()
{
    if (!ThumbnailRenderer::IsCreated()) return;

    auto& thumbRenderer = ThumbnailRenderer::Get();

    // 1단계: 이전 프레임에서 제출한 렌더링 결과물이 있으면 텍스처 복제하여 캐시에 등록
    if (m_CaptureState == ECaptureState::Rendering)
    {
        const auto loading = m_LoadingPaths.find(m_CurrentCapturingKey);
        const bool isCurrent = loading != m_LoadingPaths.end() &&
            loading->second == m_CurrentCapturingRequestId;

        if (isCurrent)
        {
            RHITexture* newTexture = thumbRenderer.CreateThumbnailTexture();
            m_LoadingPaths.erase(loading);

            const size_t byteSize = newTexture != nullptr ?
                static_cast<size_t>(MAX_THUMBNAIL_DIM) * MAX_THUMBNAIL_DIM * 4 : 0;

            InsertToCache(m_CurrentCapturingKey, newTexture, byteSize);
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
    }

    // 2단계: 대기열에서 다음 에셋 1개를 꺼내어 타입별 자동 프레이밍 렌더링 제출
    while (!m_Pending3DQueue.empty() && m_CaptureState == ECaptureState::Idle)
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

        const std::wstring relPath = Engine::PlatformPath::ToProjectRelative(path).wstring();

        auto& rm = Engine::ResourceManager::Get();
        bool submitSuccess = false;

        const bool isMat = ext == ".bammat" || ext == ".bammatinst" || ext == ".mat" ||
            filename.ends_with(".bammat.json") || filename.ends_with(".bammatinst.json");
        const bool isModel = ext == ".bammodel" || filename.ends_with(".bammodel.json");
        const bool isMesh = ext == ".bammesh" || ext == ".fbx" || ext == ".glb" || ext == ".gltf" || ext == ".obj" || filename.ends_with(".bammesh.json");
        const bool isTex = ext == ".bamtex" || filename.ends_with(".bamtex.json");
        const bool isSprite = ext == ".bamsprite" || filename.ends_with(".bamsprite.json");

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
                        InsertToCache(key, rhiTex, 0);
                        m_LoadingPaths.erase(key);
                        continue;
                    }
                }
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
            if (handle.IsValid() && handle.Get()) submitSuccess = thumbRenderer.SubmitMaterial(handle.Get());
        }
        else if (isModel)
        {
            auto handle = rm.GetResourceHandle<Engine::Model>(relPath);
            if (!handle.IsValid())
            {
                auto loaded = rm.LoadFile(relPath);
                if (loaded.IsValid()) handle = Engine::ResourceHandle<Engine::Model>(loaded);
            }
            if (handle.IsValid() && handle.Get()) submitSuccess = thumbRenderer.SubmitModel(handle.Get());
        }
        else if (isMesh)
        {
            auto handle = rm.GetResourceHandle<Engine::Mesh>(relPath);
            if (!handle.IsValid())
            {
                auto loaded = rm.LoadFile(relPath);
                if (loaded.IsValid()) handle = Engine::ResourceHandle<Engine::Mesh>(loaded);
            }
            if (handle.IsValid() && handle.Get()) submitSuccess = thumbRenderer.SubmitMesh(handle.Get());
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
            m_LoadingPaths.erase(key);
            InsertToCache(key, nullptr, 0);
        }
    }
}
#pragma endregion

#pragma region Thumbnail Management
RHITexture* AssetCache::GetThumbnail(const filesystem::path& assetPath)
{
    const std::string key = ThumbnailCacheKey(assetPath);
    if (key.empty())
        return nullptr;

    if (auto cached = m_ThumbnailCache.find(key); cached != m_ThumbnailCache.end())
    {
        // F12: 접근 시 LRU 리스트 맨 앞으로 갱신
        m_LruList.splice(m_LruList.begin(), m_LruList, cached->second.LruIt);
        return cached->second.Texture;
    }

    if (m_LoadingPaths.contains(key))
        return nullptr;

    std::string filename = Engine::WStrToStr(assetPath.filename().wstring());
    for (char& ch : filename)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    std::string ext = Engine::WStrToStr(assetPath.extension().wstring());
    for (char& ch : ext)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    const bool isImage = ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp";
    const bool isMaterial = ext == ".bammat" || ext == ".bammatinst" || ext == ".mat" ||
        filename.ends_with(".bammat.json") || filename.ends_with(".bammatinst.json");
    const bool isModelOrMesh = ext == ".bammodel" || ext == ".bammesh" || ext == ".glb" || ext == ".gltf" || ext == ".fbx" || ext == ".obj" ||
        filename.ends_with(".bammodel.json") || filename.ends_with(".bammesh.json");
    const bool isTextureOrSprite = ext == ".bamtex" || ext == ".bamsprite" ||
        filename.ends_with(".bamtex.json") || filename.ends_with(".bamsprite.json");

    if (isMaterial || isModelOrMesh || isTextureOrSprite)
    {
        const Engine::uint64 requestId = ++m_NextRequestId;
        m_LoadingPaths.emplace(key, requestId);
        m_Pending3DQueue.push(Thumbnail3DRequest{ key, requestId });
        return nullptr;
    }

    if (!isImage)
    {
        InsertToCache(key, nullptr, 0);
        return nullptr;
    }

    const Engine::uint64 requestId = ++m_NextRequestId;
    m_LoadingPaths.emplace(key, requestId);

    try
    {
        std::thread([this, key, requestId, isImage]()
            {
                ThumbnailUploadTask task;

                try
                {
                    const filesystem::path path = Engine::Utf8ToPath(key);
                    auto decoded = isImage ? LoadImageThumbnail(path) : LoadModelThumbnail(path);
                    if (decoded) task = std::move(*decoded);
                }
                catch (const std::exception& error)
                {
                    BAM_LOG(Error, "Editor", "Asset", "Thumbnail generation failed: path={}, error={}", key, error.what());
                }
                catch (...)
                {
                    BAM_LOG(Error, "Editor", "Asset", "Thumbnail generation threw an unknown exception: path={}", key);
                }

                task.AssetPath = key;
                task.RequestId = requestId;

                {
                    std::lock_guard lock(m_UploadMutex);
                    m_UploadTasks.push_back(std::move(task));
                }
            }).detach();
    }
    catch (...)
    {
        m_LoadingPaths.erase(key);
        InsertToCache(key, nullptr, 0);
        BAM_LOG(Error, "Editor", "Asset", "Failed to queue thumbnail generation: path={}", key);
    }

    return nullptr;
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
    if (key.empty())
        return;

    m_LoadingPaths.erase(key);

    const auto cached = m_ThumbnailCache.find(key);
    if (cached == m_ThumbnailCache.end())
        return;

    m_CurrentCacheBytes -= cached->second.ByteSize;
    Safe_Release(cached->second.Texture);
    m_LruList.erase(cached->second.LruIt);
    m_ThumbnailCache.erase(cached);
}

void AssetCache::ClearAll()
{
    m_LoadingPaths.clear();
    m_Pending3DQueue = {};
    m_CurrentCapturingKey.clear();
    m_CurrentCapturingRequestId = 0;
    m_CaptureState = ECaptureState::Idle;
    for (auto& [path, entry] : m_ThumbnailCache)
        Safe_Release(entry.Texture);
    m_ThumbnailCache.clear();
    m_LruList.clear();
    m_CurrentCacheBytes = 0;
    m_PendingUploadQueue.clear();

    std::vector<ThumbnailUploadTask> discarded;

    {
        std::lock_guard lock(m_UploadMutex);
        discarded.swap(m_UploadTasks);
        m_PendingInvalidations.clear();
    }
}

std::optional<AssetCache::ThumbnailUploadTask> AssetCache::LoadImageThumbnail(const filesystem::path& assetPath)
{
    const auto fullPath = Engine::PlatformPath::ResolveProjectRelative(assetPath);
    std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return std::nullopt;

    const std::streamsize size = file.tellg();
    if (size <= 0 || size > static_cast<std::streamsize>((std::numeric_limits<int>::max)()))
        return std::nullopt;

    file.seekg(0, std::ios::beg);

    std::vector<stbi_uc> encodedData(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(encodedData.data()), size))
        return std::nullopt;

    int32 width = 0, height = 0, channels = 0;
    stbi_uc* data = stbi_load_from_memory(encodedData.data(), static_cast<int>(size),
        &width, &height, &channels, 4);

    if (data)
    {
        ThumbnailUploadTask task;
        task.AssetPath = ThumbnailCacheKey(assetPath);

        // F12: 원본 이미지가 썸네일 해상도(128×128)를 초과하면 종횡비를 유지하며 다운샘플링
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
            task.Data = DownsampleRGBA(data, width, height, targetW, targetH);
            stbi_image_free(data);
        }
        else
        {
            task.Width = width;
            task.Height = height;
            task.Data.assign(data, data + (static_cast<size_t>(width) * height * 4));
            stbi_image_free(data);
        }

        return task;
    }
    return std::nullopt;
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
#pragma endregion

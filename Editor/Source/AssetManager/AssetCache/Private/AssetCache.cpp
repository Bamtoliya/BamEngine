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

namespace
{
    std::string ThumbnailCacheKey(const filesystem::path& path)
    {
        if (path.empty())
            return {};

        std::error_code ec;
        const auto absolutePath = path.is_absolute() ? path : filesystem::absolute(path, ec);
        if (ec)
            return {};

        return absolutePath.lexically_normal().generic_string();
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
        ClearCache(filesystem::path(path));

    RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;

    for (auto& task : tasks)
    {
        const auto loading = m_LoadingPaths.find(task.AssetPath);

        if (loading == m_LoadingPaths.end() || loading->second != task.RequestId)
        {
            BAM_LOG(Trace, "Editor", "Asset",
                "Discarded stale thumbnail: path={}, request={}", task.AssetPath, task.RequestId);
            continue;
        }

        m_LoadingPaths.erase(loading);

        RHITexture* texture = nullptr;

        if (rhi && task.Width > 0 && task.Height > 0 && !task.Data.empty())
        {
            RHITextureDesc desc = {};
            desc.width = task.Width;
            desc.height = task.Height;
            desc.data = task.Data.data();
            desc.dataSize = static_cast<uint32>(task.Data.size());

            if (task.Data.size() == desc.dataSize)
                texture = rhi->CreateTexture(desc);
        }

        // 실패 결과도 nullptr로 캐시합니다.
        const auto inserted = m_ThumbnailCache.emplace(task.AssetPath, texture);
        if (!inserted.second)
            Safe_Release(texture);
    }
    // 머티리얼 1프레임 오프스크린 캡처 파이프라인 처리
    ProcessPendingMaterialThumbnail();
}

void AssetCache::ProcessPendingMaterialThumbnail()
{
    if (!ThumbnailRenderer::IsCreated()) return;

    auto& thumbRenderer = ThumbnailRenderer::Get();

    // 1단계: 이전 프레임에서 렌더링 제출한 결과물이 있으면 복제하여 캐시에 등록
    if (m_CaptureState == ECaptureState::Rendering)
    {
        if (!m_CurrentCapturingKey.empty())
        {
            RHITexture* newTexture = thumbRenderer.CreateThumbnailTexture();
            m_LoadingPaths.erase(m_CurrentCapturingKey);

            if (newTexture)
            {
                const auto inserted = m_ThumbnailCache.emplace(m_CurrentCapturingKey, newTexture);
                if (!inserted.second)
                    Safe_Release(newTexture);
            }
            else
            {
                m_ThumbnailCache.emplace(m_CurrentCapturingKey, nullptr);
            }
        }

        m_CurrentCapturingKey.clear();
        m_CaptureState = ECaptureState::Idle;
    }

    // 2단계: 대기열에서 다음 머티리얼 1개를 꺼내어 128x128 뷰포트에 렌더링 제출
    while (!m_PendingMaterialQueue.empty() && m_CaptureState == ECaptureState::Idle)
    {
        std::string key = m_PendingMaterialQueue.front();
        m_PendingMaterialQueue.pop();

        // 이미 무효화되었거나 캐시가 존재하면 스킵
        if (!m_LoadingPaths.contains(key) || m_ThumbnailCache.contains(key))
            continue;

        std::filesystem::path path(key);
        std::error_code ec;
        const std::wstring relPath = std::filesystem::relative(path, ec).wstring();
        if (ec) continue;

        auto& rm = Engine::ResourceManager::Get();
        auto handle = rm.GetResourceHandle<Engine::MaterialInterface>(relPath);
        if (!handle.IsValid())
        {
            auto loadedHandle = rm.LoadFile(relPath);
            if (loadedHandle.IsValid())
                handle = Engine::ResourceHandle<Engine::MaterialInterface>(loadedHandle);
        }

        auto* material = handle.Get();
        if (material && thumbRenderer.SubmitMaterial(material))
        {
            m_CurrentCapturingKey = key;
            m_CaptureState = ECaptureState::Rendering;
            break;
        }
        else
        {
            // 3D 렌더링을 지원하지 않는 머티리얼(UI/Sprite 등)이거나 로드 실패 시 nullptr 캐시 (기본 아이콘 표시)
            m_LoadingPaths.erase(key);
            m_ThumbnailCache.emplace(key, nullptr);
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

    if (const auto cached = m_ThumbnailCache.find(key); cached != m_ThumbnailCache.end())
        return cached->second;

    if (m_LoadingPaths.contains(key))
        return nullptr;

    std::string filename = assetPath.filename().string();
    for (char& ch : filename)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    std::string ext = assetPath.extension().string();
    for (char& ch : ext)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));

    const bool isImage = ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".bmp";
    const bool isModel = false;
    const bool isMaterial = ext == ".bammat" || ext == ".bammatinst" || ext == ".mat" ||
        filename.ends_with(".bammat.json") || filename.ends_with(".bammatinst.json");

    if (isMaterial)
    {
        const Engine::uint64 requestId = ++m_NextRequestId;
        m_LoadingPaths.emplace(key, requestId);
        m_PendingMaterialQueue.push(key);
        return nullptr;
    }

    if (!isImage && !isModel)
    {
        m_ThumbnailCache.emplace(key, nullptr);
        return nullptr;
    }

    const Engine::uint64 requestId = ++m_NextRequestId;
    m_LoadingPaths.emplace(key, requestId);

    try
    {
        AssetManager::Get().ExecuteAsync([this, key, requestId, isImage]() -> EResult
            {
                ThumbnailUploadTask task;
                EResult result = EResult::Success;

                try
                {
                    const filesystem::path path(key);
                    auto decoded = isImage ? LoadImageThumbnail(path) : LoadModelThumbnail(path);

                    if (decoded)
                        task = std::move(*decoded);
                }
                catch (const std::exception& error)
                {
                    BAM_LOG(Error, "Editor", "Asset",
                        "Thumbnail generation failed: path={}, error={}", key, error.what());
                    result = EResult::Fail;
                }
                catch (...)
                {
                    BAM_LOG(Error, "Editor", "Asset",
                        "Thumbnail generation threw an unknown exception: path={}", key);
                    result = EResult::Fail;
                }

                task.AssetPath = key;
                task.RequestId = requestId;

                {
                    std::lock_guard lock(m_UploadMutex);
                    m_UploadTasks.push_back(std::move(task));
                }

                return result;
            });
    }
    catch (...)
    {
        m_LoadingPaths.erase(key);
        m_ThumbnailCache.emplace(key, nullptr);
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

    // 캐시가 아직 없더라도 진행 중인 요청은 무효화해야 합니다.
    m_LoadingPaths.erase(key);

    const auto cached = m_ThumbnailCache.find(key);
    if (cached == m_ThumbnailCache.end())
        return;

    Safe_Release(cached->second);
    m_ThumbnailCache.erase(cached);
}

void AssetCache::ClearAll()
{
    m_LoadingPaths.clear();
    m_PendingMaterialQueue = {};
    m_CurrentCapturingKey.clear();
    m_CaptureState = ECaptureState::Idle;
    for (auto& [path, texture] : m_ThumbnailCache)
        Safe_Release(texture);
    m_ThumbnailCache.clear();

    std::vector<ThumbnailUploadTask> discarded;

    {
        std::lock_guard lock(m_UploadMutex);
        discarded.swap(m_UploadTasks);
        m_PendingInvalidations.clear();
    }

    // m_NextRequestId는 초기화하지 않습니다.
    // 이전 작업의 번호와 새 요청의 번호가 다시 같아지는 것을 막습니다.
}

std::optional<AssetCache::ThumbnailUploadTask> AssetCache::LoadImageThumbnail(const filesystem::path& assetPath)
{
    int32 width, height, channels;
    stbi_uc* data = stbi_load(assetPath.string().c_str(), &width, &height, &channels, 4);

    if (data)
    {
        ThumbnailUploadTask task;
        task.AssetPath = assetPath.string();
        task.Width = width;
        task.Height = height;
        task.Data.assign(data, data + (width * height * 4));
        stbi_image_free(data);
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
        SIZE size = { 256, 256 };
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

            std::vector<uint8_t> pixels(width * height * 4);
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
            task.AssetPath = assetPath.string();
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

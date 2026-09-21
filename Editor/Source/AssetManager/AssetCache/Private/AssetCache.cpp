#pragma once
#include "AssetCache.h"
#include <stb_image.h>
#include "Renderer.h"
#include "RHI.h"
#include <shlobj.h> // Windows Shell API
#include "AssetManager.h"
#include "RHITexture.h"

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
	{
		std::lock_guard lock(m_UploadMutex);
		tasks = std::move(m_UploadTasks);
	}

	for (auto& task : tasks)
	{
		// 디코딩 실패 처리
		if (task.Width == 0 || task.Height == 0)
		{
			m_ThumbnailCache[task.AssetPath] = nullptr;
			m_LoadingPaths.erase(task.AssetPath);
			continue;
		}

		RHITextureDesc desc = {};
		desc.width = task.Width;
		desc.height = task.Height;
		desc.data = task.Data.data();
		desc.dataSize = (uint32)task.Data.size();

		RHITexture* rhiTexture = Renderer::Get().GetRHI()->CreateTexture(desc);
		m_ThumbnailCache.emplace(task.AssetPath, rhiTexture);	
		m_LoadingPaths.erase(task.AssetPath);
	}
}
#pragma endregion

#pragma region Thumbnail Management
RHITexture* AssetCache::GetThumbnail(const filesystem::path& assetPath)
{
    std::string pathStr = assetPath.string();

    // 1. 이미 캐시에 있다면 즉시 반환
    auto it = m_ThumbnailCache.find(pathStr);
    if (it != m_ThumbnailCache.end())
        return it->second;

    // 2. 이미 로딩 중이라면 더미(nullptr) 반환
    if (m_LoadingPaths.count(pathStr))
        return nullptr;

    m_LoadingPaths.insert(pathStr);

    // 3. 백그라운드 디코딩 태스크 예약
    std::string ext = assetPath.extension().string();
    for (auto& c : ext) c = tolower(c);

    AssetManager::Get().ExecuteAsync([this, pathStr, ext]() -> EResult {
        std::optional<ThumbnailUploadTask> task;
        if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".bmp")
        {
            task = LoadImageThumbnail(pathStr);
        }
        else if (ext == ".fbx" || ext == ".obj" || ext == ".gltf")
        {
            task = LoadModelThumbnail(pathStr);
        }

        if (task)
        {
            std::lock_guard lock(m_UploadMutex);
            m_UploadTasks.push_back(std::move(task.value()));
        }
        else
        {
            // 실패 처리: 메인 쓰레드에서 nullptr 캐싱하도록 Width=0인 빈 태스크 전달
            ThumbnailUploadTask failTask;
            failTask.AssetPath = pathStr;
            failTask.Width = 0;
            std::lock_guard lock(m_UploadMutex);
            m_UploadTasks.push_back(std::move(failTask));
        }
        return EResult::Success;
    });

    return nullptr;
}

void AssetCache::ClearCache(const filesystem::path& assetPath)
{
    auto it = m_ThumbnailCache.find(assetPath.string());

    if (it == m_ThumbnailCache.end())
        return;

    Safe_Release(it->second);
    m_ThumbnailCache.erase(it);
}
void AssetCache::ClearAll()
{
	for (auto& [path, texture] : m_ThumbnailCache)
	{
		Safe_Release(texture);
	}
	m_ThumbnailCache.clear();
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

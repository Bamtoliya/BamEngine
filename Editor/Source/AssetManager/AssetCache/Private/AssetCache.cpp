#pragma once
#include "AssetCache.h"
#include <stb_image.h>
#include "Renderer.h"
#include "RHI.h"
#include <shlobj.h> // Windows Shell API

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
#pragma endregion

#pragma region Thumbnail Management
void* AssetCache::GetThumbnail(const filesystem::path& assetPath)
{
    std::string pathStr = assetPath.string();

    // 1. 이미 캐시에 있다면 즉시 반환
    auto it = m_ThumbnailCache.find(pathStr);
    if (it != m_ThumbnailCache.end())
        return it->second;

    // 2. 확장자에 따라 적절한 로더 호출
    std::string ext = assetPath.extension().string();
    for (auto& c : ext) c = tolower(c);

    void* result = nullptr;
    if (ext == ".png" || ext == ".jpg" || ext == ".tga" || ext == ".bmp")
    {
        result = LoadImageThumbnail(assetPath);
    }
    else if (ext == ".fbx" || ext == ".obj" || ext == ".gltf")
    {
        result = LoadModelThumbnail(assetPath);
    }

    // 3. 결과 캐싱 (실패 시 nullptr 저장하여 중복 로드 방지)
    m_ThumbnailCache[pathStr] = result;
    return result;
}

void AssetCache::ClearCache(const filesystem::path& assetPath)
{
    std::string pathStr = assetPath.string();
    if (m_ThumbnailTextures.count(pathStr))
    {
        Safe_Release(m_ThumbnailTextures[pathStr]);
        m_ThumbnailTextures.erase(pathStr);
    }
    m_ThumbnailCache.erase(pathStr);
}
void AssetCache::ClearAll()
{
	for (auto& [path, texture] : m_ThumbnailTextures)
	{
		Safe_Release(texture);
	}
	m_ThumbnailTextures.clear();
	m_ThumbnailCache.clear();
}

void* AssetCache::LoadImageThumbnail(const filesystem::path& assetPath)
{
    int32 width, height, channels;
    stbi_uc* data = stbi_load(assetPath.string().c_str(), &width, &height, &channels, 4);

    if (data)
    {
        tagRHITextureDesc desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.Data = data;
        desc.DataSize = width * height * 4;

        RHITexture* rhiTexture = Renderer::Get().GetRHI()->CreateTextureFromMemory(desc);
        stbi_image_free(data);

        if (rhiTexture)
        {
            m_ThumbnailTextures[assetPath.string()] = rhiTexture;
            return (void*)(size_t)(rhiTexture->GetNativeHandle());
        }
    }
    return nullptr;
}
void* AssetCache::LoadModelThumbnail(const filesystem::path& assetPath)
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

            tagRHITextureDesc desc = {};
            desc.Width = width;
            desc.Height = height;
            desc.Data = pixels.data();
            desc.DataSize = (uint32)pixels.size();

            RHITexture* rhiTexture = Renderer::Get().GetRHI()->CreateTextureFromMemory(desc);
            if (rhiTexture)
            {
                m_ThumbnailTextures[assetPath.string()] = rhiTexture;
                return (void*)(size_t)(rhiTexture->GetNativeHandle());
            }
        }
    }
    return nullptr;
}
#pragma endregion

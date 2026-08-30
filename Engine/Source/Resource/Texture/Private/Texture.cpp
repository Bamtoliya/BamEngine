#pragma once

#include "Texture.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "SerializationHelper.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

ETextureFormat ToTextureFormat(uint32 channels, bool sRGB)
{
	switch (channels)
	{
	case 1: return ETextureFormat::R8_UNORM;
	case 2: return ETextureFormat::R8G8_UNORM;
	case 4: return sRGB ? ETextureFormat::B8G8R8A8_UNORM_SRGB : ETextureFormat::R8G8B8A8_UNORM;
	default: return ETextureFormat::UNKNOWN;
	}
}

#pragma region Constructor&Destructor
EResult Texture::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;
	CAST_DESC
	if (m_Path.empty()) return EResult::Success;

	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi) return EResult::Fail;


	std::string pathStr = std::filesystem::path(m_Path).string();

	int32 width, height, originalChannels;
    if (!stbi_info(pathStr.c_str(), &width, &height, &originalChannels))
    {
        fmt::print(stderr, "Failed to get texture info: {}\n", pathStr);
        return EResult::Fail;
    }

    uint32 channels = (originalChannels == 3) ? 4 : originalChannels;
	stbi_uc* pixels = stbi_load(pathStr.c_str(), &width, &height, &originalChannels, channels);

    if (!pixels) return EResult::Fail;

	RHITextureDesc txDesc = {};
    txDesc.width = width;
	txDesc.height = height;
	txDesc.depth = 1;
	txDesc.mipLevels = 1;
	txDesc.arraySize = 1;
	txDesc.format = ToTextureFormat(channels, desc->sRGB);
	txDesc.dimension = desc->dimension;
	txDesc.usage = desc->usage;
	txDesc.sRGB = desc->sRGB;

	txDesc.data = pixels;
	txDesc.dataSize = width * height * 4;

	m_RHITexture = rhi->CreateTexture(txDesc);
    stbi_image_free(pixels);
	return m_RHITexture ?  EResult::Success : EResult::Fail;
}

Texture* Texture::Create(void* arg)
{
	Texture* instance = new Texture();
	if (IsFailure(instance->Initialize(arg)))
	{
		delete instance;
		return nullptr;
	}
	return instance;
}

Texture* Texture::Create(const TextureBinaryHeader& header, const vector<uint8>& pixelData, const wstring& key)
{
	Texture* instance = new Texture();
	instance->m_CachedHeader = header;
	instance->m_TempData = pixelData;

    if (!key.empty())
    {
        instance->m_Key = NormalizePath(key);
        instance->m_Path = NormalizePath(key);
    }
	return instance;
}

void Texture::Free()
{
	Safe_Release(m_RHITexture);
}
#pragma endregion

#pragma region Bind
EResult Texture::Bind(uint32 slot)
{
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi || !m_RHITexture) return EResult::Fail;
	return rhi->BindTexture(m_RHITexture, slot);
}
#pragma endregion

void Texture::Serialize(Archive& ar)
{
    Resource::Serialize(ar);

    if (m_RHITexture)
    {
        m_CachedHeader.width = m_RHITexture->GetWidth();
        m_CachedHeader.height = m_RHITexture->GetHeight();
        m_CachedHeader.depth = m_RHITexture->GetDepth();
        m_CachedHeader.mipLevels = m_RHITexture->GetMipLevels();
        m_CachedHeader.arraySize = m_RHITexture->GetArraySize();
        m_CachedHeader.format = m_RHITexture->GetFormat();
        m_CachedHeader.dimension = m_RHITexture->GetDimension();
    }
    m_CachedHeader.dataSize = static_cast<uint32>(m_TempData.size());

    if (ar.PushScope("TextureHeader"))
    {
        SerializationHelper::SerializeStaticType(ar, m_CachedHeader);
        ar.PopScope();
    }

    if (!m_TempData.empty())
    {
        ar.ProcessRaw("PixelData", m_TempData.data(), m_TempData.size());
    }
}

void Texture::Deserialize(Archive& ar)
{
    Resource::Deserialize(ar);

    if (ar.PushScope("TextureHeader"))
    {
        SerializationHelper::SerializeStaticType(ar, m_CachedHeader);
        ar.PopScope();
    }

    if (m_CachedHeader.dataSize > 0)
    {
        std::vector<uint8> rawData(m_CachedHeader.dataSize);
        ar.ProcessRaw("PixelData", rawData.data(), rawData.size());

        RHITextureDesc desc = {};
        desc.width = m_CachedHeader.width;
        desc.height = m_CachedHeader.height;
        desc.depth = m_CachedHeader.depth;
        desc.mipLevels = m_CachedHeader.mipLevels;
        desc.arraySize = m_CachedHeader.arraySize;
        desc.format = m_CachedHeader.format;
        desc.dimension = m_CachedHeader.dimension;
        desc.data = rawData.data();
        desc.dataSize = m_CachedHeader.dataSize;

        Safe_Release(m_RHITexture);
        m_RHITexture = Renderer::Get().GetRHI()->CreateTexture(desc);
    }
}
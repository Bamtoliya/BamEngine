#pragma once

#include <archive/ArchiveInterface.h>
#include "Texture.h"
#include "Renderer.h"
#include "ResourceManager.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

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

constexpr uint32 TextureArchiveFormatVersion = 1;

[[nodiscard]]
archive::ArchiveResult ValidateTextureHeader(const TextureBinaryHeader& header, f32 ppu, uint32& byteCount)
{
	if (header.dimension != ETextureDimension::Texture2D ||
		header.depth != 1 || header.mipLevels != 1 || header.arraySize != 1)
	{
		return { archive::ArchiveErrorCode::UnsupportedType, "Texture.TextureHeader.Layout" };
	}

	if (header.format != ETextureFormat::R8G8B8A8_UNORM)
		return { archive::ArchiveErrorCode::UnsupportedType, "Texture.TextureHeader.format" };

	if (header.width == 0 || header.height == 0)
		return { archive::ArchiveErrorCode::InvalidValue, "Texture.TextureHeader.Size" };

	if (!std::isfinite(ppu) || ppu <= 0.f)
		return { archive::ArchiveErrorCode::InvalidValue, "Texture.PixelPerUnit" };

	const uint64 rowBytes = static_cast<uint64>(header.width) * 4;

	if (rowBytes > std::numeric_limits<uint32>::max() / header.height)
		return { archive::ArchiveErrorCode::InvalidValue, "Texture.TextureHeader.dataSize" };

	byteCount = static_cast<uint32>(rowBytes * header.height);
	return {};
}

#pragma region Constructor&Destructor
EResult Texture::Initialize(void* arg)
{
	const EResult result = Resource::Initialize(arg);
	if (result != EResult::Success)
		return result;

	CAST_DESC

		if (m_Path.empty())
			return EResult::InvalidArgument;

	if (desc->dimension != ETextureDimension::Texture2D ||
		desc->usage != ETextureUsage::Sampler || desc->sRGB)
	{
		return EResult::NotImplemented;
	}

	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi)
		return EResult::Fail;

	const std::string path = std::filesystem::path(m_Path).string();
	int width = 0;
	int height = 0;
	int originalChannels = 0;

	std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
		stbi_load(path.c_str(), &width, &height, &originalChannels, STBI_rgb_alpha), stbi_image_free);

	if (!pixels || width <= 0 || height <= 0)
		return EResult::Fail;

	TextureBinaryHeader header;
	header.width = static_cast<uint32>(width);
	header.height = static_cast<uint32>(height);
	header.depth = 1;
	header.mipLevels = 1;
	header.arraySize = 1;
	header.format = ETextureFormat::R8G8B8A8_UNORM;
	header.dimension = ETextureDimension::Texture2D;

	uint32 byteCount = 0;
	if (!ValidateTextureHeader(header, m_PixelPerUnit, byteCount))
		return EResult::InvalidArgument;

	header.dataSize = byteCount;
	vector<uint8> pixelData(pixels.get(), pixels.get() + byteCount);

	RHITextureDesc textureDesc;
	textureDesc.width = header.width;
	textureDesc.height = header.height;
	textureDesc.format = header.format;
	textureDesc.dimension = header.dimension;
	textureDesc.usage = ETextureUsage::Sampler;
	textureDesc.data = pixelData.data();
	textureDesc.dataSize = byteCount;

	RHITexture* texture = rhi->CreateTexture(textureDesc);
	if (!texture)
		return EResult::Fail;

	m_CachedHeader = header;
	m_TempData = std::move(pixelData);
	m_RHITexture = texture;
	return EResult::Success;
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

#pragma region Save&Load
archive::ArchiveResult Texture::WriteArchive(archive::IArchive& ar) const
{
	uint32 byteCount = 0;
	auto result = ValidateTextureHeader(m_CachedHeader, m_PixelPerUnit, byteCount);
	if (!result)
		return result;

	if (m_CachedHeader.dataSize != byteCount || m_TempData.size() != byteCount)
		return { archive::ArchiveErrorCode::InvalidValue, "Texture.PixelData" };

	return ar.WriteObject("Texture", *this,
		[](archive::IArchive& destination, const Texture& texture) -> archive::ArchiveResult
		{
			ResourceBinaryHeader assetHeader;
			assetHeader.ResourceType = texture.m_ResourceType;
			assetHeader.Version = TextureArchiveFormatVersion;

			auto result = destination.WriteObject("AssetHeader", assetHeader,
				[](archive::IArchive& fields, const ResourceBinaryHeader& header) -> archive::ArchiveResult
				{
					archive::ArchiveResult result;
					const auto write = [&](std::string_view key, const auto& value)
						{
							if (result)
								result = fields.Write(key, value);
						};

					write("MagicNumber", header.MagicNumber);
					write("ResourceType", header.ResourceType);
					write("Version", header.Version);
					return result;
				});

			if (!result)
				return result;

			result = destination.WriteObject("TextureHeader", texture.m_CachedHeader,
				[](archive::IArchive& fields, const TextureBinaryHeader& header) -> archive::ArchiveResult
				{
					archive::ArchiveResult result;
					const auto write = [&](std::string_view key, const auto& value)
						{
							if (result)
								result = fields.Write(key, value);
						};

					write("width", header.width);
					write("height", header.height);
					write("depth", header.depth);
					write("mipLevels", header.mipLevels);
					write("arraySize", header.arraySize);
					write("format", header.format);
					write("dimension", header.dimension);
					write("dataSize", header.dataSize);
					return result;
				});

			if (!result)
				return result;

			result = destination.Write("PixelPerUnit", texture.m_PixelPerUnit);
			if (!result)
				return result;

			return destination.WriteArray("PixelData", texture.m_TempData);
		});
}
archive::ArchiveResult Texture::ReadArchive(const archive::IArchive& ar)
{
	if (m_RHITexture)
		return { archive::ArchiveErrorCode::InvalidOperation, "Texture.AlreadyUploaded" };

	ResourceBinaryHeader assetHeader;
	TextureBinaryHeader textureHeader;
	f32 pixelPerUnit = DEFAULT_PPU;
	vector<uint8> pixelData;

	auto result = ar.ReadObject("Texture", textureHeader,
		[&assetHeader, &pixelPerUnit, &pixelData](archive::IArchive& source, TextureBinaryHeader& header)
		-> archive::ArchiveResult
		{
			auto result = source.ReadObject("AssetHeader", assetHeader,
				[](archive::IArchive& fields, ResourceBinaryHeader& header) -> archive::ArchiveResult
				{
					archive::ArchiveResult result;
					const auto read = [&](std::string_view key, auto& value)
						{
							if (result)
								result = fields.Read(key, value);
						};

					read("MagicNumber", header.MagicNumber);
					read("ResourceType", header.ResourceType);
					read("Version", header.Version);
					return result;
				});

			if (!result)
				return result;

			if (assetHeader.MagicNumber != ENGINE_ASSET_MAGIC)
				return { archive::ArchiveErrorCode::InvalidDocument, "Texture.AssetHeader.MagicNumber" };

			if (assetHeader.ResourceType != EResourceType::Texture)
				return { archive::ArchiveErrorCode::InvalidValue, "Texture.AssetHeader.ResourceType" };

			if (assetHeader.Version != TextureArchiveFormatVersion)
				return { archive::ArchiveErrorCode::UnsupportedType, "Texture.AssetHeader.Version" };

			result = source.ReadObject("TextureHeader", header,
				[](archive::IArchive& fields, TextureBinaryHeader& header) -> archive::ArchiveResult
				{
					archive::ArchiveResult result;
					const auto read = [&](std::string_view key, auto& value)
						{
							if (result)
								result = fields.Read(key, value);
						};

					read("width", header.width);
					read("height", header.height);
					read("depth", header.depth);
					read("mipLevels", header.mipLevels);
					read("arraySize", header.arraySize);
					read("format", header.format);
					read("dimension", header.dimension);
					read("dataSize", header.dataSize);
					return result;
				});

			if (!result)
				return result;

			result = source.Read("PixelPerUnit", pixelPerUnit);
			if (!result)
				return result;

			uint32 byteCount = 0;
			result = ValidateTextureHeader(header, pixelPerUnit, byteCount);
			if (!result)
				return result;

			if (header.dataSize != byteCount)
				return { archive::ArchiveErrorCode::InvalidValue, "Texture.TextureHeader.dataSize" };

			std::size_t elementCount = 0;
			result = source.GetArraySize("PixelData", elementCount);
			if (!result)
				return result;

			if (elementCount != byteCount)
				return { archive::ArchiveErrorCode::InvalidValue, "Texture.PixelData" };

			return source.ReadArray("PixelData", pixelData);
		});

	if (!result)
		return result;

	// Renderer 초기화 후, 프레임 렌더링 전에 호출합니다.
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi)
		return { archive::ArchiveErrorCode::InvalidOperation, "Texture.RHI" };

	RHITextureDesc textureDesc;
	textureDesc.width = textureHeader.width;
	textureDesc.height = textureHeader.height;
	textureDesc.depth = textureHeader.depth;
	textureDesc.mipLevels = textureHeader.mipLevels;
	textureDesc.arraySize = textureHeader.arraySize;
	textureDesc.format = textureHeader.format;
	textureDesc.dimension = textureHeader.dimension;
	textureDesc.usage = ETextureUsage::Sampler;
	textureDesc.data = pixelData.data();
	textureDesc.dataSize = textureHeader.dataSize;

	RHITexture* texture = rhi->CreateTexture(textureDesc);
	if (!texture)
		return { archive::ArchiveErrorCode::InvalidOperation, "Texture.GPUUpload" };

	m_CachedHeader = textureHeader;
	m_TempData = std::move(pixelData);
	m_PixelPerUnit = pixelPerUnit;
	m_RHITexture = texture;
	IncreaseVersion();
	return {};
}
#pragma endregion

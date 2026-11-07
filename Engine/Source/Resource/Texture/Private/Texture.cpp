#pragma once

#include <archive/ArchiveInterface.h>
#include "Texture.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "Runtime.h"
#include "PlatformPath.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
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
	const bool isValid2D = (header.dimension == ETextureDimension::Texture2D && header.depth == 1);
	const bool isValid3D = (header.dimension == ETextureDimension::Texture3D && header.depth >= 1);
	if ((!isValid2D && !isValid3D) || header.mipLevels < 1 || header.arraySize != 1)
		return { archive::ArchiveErrorCode::UnsupportedType, "Texture.TextureHeader.Layout" };
	if (header.format != ETextureFormat::R8G8B8A8_UNORM)
		return { archive::ArchiveErrorCode::UnsupportedType, "Texture.TextureHeader.format" };
	if (header.width == 0 || header.height == 0 || header.depth == 0)
		return { archive::ArchiveErrorCode::InvalidValue, "Texture.TextureHeader.Size" };
	if (!std::isfinite(ppu) || ppu <= 0.f)
		return { archive::ArchiveErrorCode::InvalidValue, "Texture.PixelPerUnit" };

	uint64 totalBytes = 0;
	if (header.dimension == ETextureDimension::Texture2D)
	{
		uint32 curW = header.width;
		uint32 curH = header.height;
		for (uint32 m = 0; m < header.mipLevels; ++m)
		{
			totalBytes += static_cast<uint64>(curW) * 4 * curH;
			curW = (std::max)(1u, curW >> 1);
			curH = (std::max)(1u, curH >> 1);
		}
	}
	else
	{
		const uint64 sliceBytes = static_cast<uint64>(header.width) * 4 * header.height;
		totalBytes = sliceBytes * header.depth;
	}

	if (totalBytes > std::numeric_limits<uint32>::max())
		return { archive::ArchiveErrorCode::InvalidValue, "Texture.TextureHeader.dataSize" };
	byteCount = static_cast<uint32>(totalBytes);
	return {};
}

uint32 Texture::CalculateMipLevels(uint32 width, uint32 height)
{
	if (width == 0 || height == 0) return 1;
	uint32 maxDim = (std::max)(width, height);
	uint32 levels = 1;
	while (maxDim > 1)
	{
		maxDim >>= 1;
		++levels;
	}
	return levels;
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

	std::filesystem::path fullPath = PlatformPath::ResolveUri(m_Path);

	std::ifstream file(fullPath, std::ios::binary | std::ios::ate);
	if (!file.is_open())
		return EResult::FileNotFound;

	const std::streamsize size = file.tellg();
	if (size <= 0 || size > static_cast<std::streamsize>((std::numeric_limits<int>::max)()))
		return EResult::InvalidArgument;

	file.seekg(0, std::ios::beg);

	vector<uint8> buffer(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		return EResult::Fail;

	return InitializeFromEncodedMemory(buffer, desc->generateMips);
}

EResult Texture::InitializeFromEncodedMemory(const vector<uint8>& encodedData, bool generateMips)
{
	if (encodedData.empty() || encodedData.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
		return EResult::InvalidArgument;

	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	if (!rhi) return EResult::Fail;

	const int encodedSize = static_cast<int>(encodedData.size());
	int width = 0;
	int height = 0;
	int channels = 0;

	if (!stbi_info_from_memory(encodedData.data(), encodedSize, &width, &height, &channels))
		return EResult::Fail;

	if (width <= 0 || height <= 0)
		return EResult::InvalidArgument;

	TextureBinaryHeader header{};
	header.width = static_cast<uint32>(width);
	header.height = static_cast<uint32>(height);
	header.depth = 1;
	header.mipLevels = (generateMips && (width > 1 || height > 1)) ? CalculateMipLevels(header.width, header.height) : 1;
	header.arraySize = 1;
	header.format = ETextureFormat::R8G8B8A8_UNORM;
	header.dimension = ETextureDimension::Texture2D;

	uint32 byteCount = 0;
	if (!ValidateTextureHeader(header, m_PixelPerUnit, byteCount))
		return EResult::InvalidArgument;

	int decodedWidth = 0;
	int decodedHeight = 0;

	std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
		stbi_load_from_memory(encodedData.data(), encodedSize, &decodedWidth, &decodedHeight, &channels,
			STBI_rgb_alpha),
		stbi_image_free);

	if (!pixels || decodedWidth != width || decodedHeight != height)
		return EResult::Fail;

	header.dataSize = byteCount;
	vector<uint8> pixelData(byteCount);

	const uint32 mip0Bytes = header.width * header.height * 4;
	std::memcpy(pixelData.data(), pixels.get(), mip0Bytes);

	if (header.mipLevels > 1)
	{
		uint32 prevOffset = 0;
		uint32 prevW = header.width;
		uint32 prevH = header.height;

		for (uint32 m = 1; m < header.mipLevels; ++m)
		{
			const uint32 curW = (std::max)(1u, header.width >> m);
			const uint32 curH = (std::max)(1u, header.height >> m);
			const uint32 curOffset = prevOffset + prevW * prevH * 4;

			const uint8* prevPixels = pixelData.data() + prevOffset;
			uint8* curPixels = pixelData.data() + curOffset;

			stbir_resize_uint8_linear(prevPixels, static_cast<int>(prevW), static_cast<int>(prevH), 0,
				curPixels, static_cast<int>(curW), static_cast<int>(curH), 0,
				STBIR_RGBA);

			prevOffset = curOffset;
			prevW = curW;
			prevH = curH;
		}
	}

	RHITextureDesc textureDesc{};
	textureDesc.width = header.width;
	textureDesc.height = header.height;
	textureDesc.depth = header.depth;
	textureDesc.mipLevels = header.mipLevels;
	textureDesc.arraySize = header.arraySize;
	textureDesc.format = header.format;
	textureDesc.dimension = header.dimension;
	textureDesc.usage = ETextureUsage::Sampler;
	textureDesc.data = pixelData.data();
	textureDesc.dataSize = byteCount;

	RHITexture* texture = rhi->CreateTexture(textureDesc);
	if (!texture) return EResult::Fail;

	Safe_Release(m_RHITexture);
	m_RHITexture = texture;
	m_CachedHeader = header;
	m_TempData = std::move(pixelData);
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

Texture* Texture::CreateFromRHITexture(RHITexture* rhiTexture, const wstring& key)
{
	if (!rhiTexture) return nullptr;
	Texture* instance = new Texture();
	instance->m_RHITexture = rhiTexture;
	instance->m_CachedHeader.width = rhiTexture->GetWidth();
	instance->m_CachedHeader.height = rhiTexture->GetHeight();
	instance->m_CachedHeader.depth = rhiTexture->GetDepth();
	instance->m_CachedHeader.format = rhiTexture->GetFormat();
	instance->m_CachedHeader.dimension = rhiTexture->GetDimension();
	if (!key.empty())
	{
		instance->m_Key = NormalizePath(key);
		instance->m_Path = NormalizePath(key);
	}
	return instance;
}

Texture* Texture::CreateFromEncodedMemory(const vector<uint8>& encodedData, const wstring& key, bool generateMips)
{
	std::unique_ptr<Texture> instance(new Texture());
	instance->m_Key = NormalizePath(key);

	if (IsFailure(instance->InitializeFromEncodedMemory(encodedData, generateMips)))
	{
		instance->Free();
		return nullptr;
	}

	return instance.release();
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
EResult Texture::Reload(const std::filesystem::path& filePath)
{
	std::filesystem::path targetPath = filePath.empty() ? std::filesystem::path(m_Path) : filePath;
	if (targetPath.empty()) return EResult::InvalidArgument;
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi) return EResult::Fail;

	targetPath = PlatformPath::ResolveUri(targetPath);

	const auto path = Engine::WStrToStr(targetPath.wstring());

	std::ifstream file(targetPath, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		ENGINE_LOG_ERROR("[Texture Hot Reload] Image file not found: {}", targetPath.string());
		return EResult::FileNotFound;
	}

	const std::streamsize size = file.tellg();
	if (size <= 0 || size > static_cast<std::streamsize>((std::numeric_limits<int>::max)()))
		return EResult::InvalidArgument;

	file.seekg(0, std::ios::beg);

	vector<uint8> buffer(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		return EResult::Fail;

	const EResult result = ReloadFromEncodedMemory(buffer);
	if (IsFailure(result))
	{
		ENGINE_LOG_ERROR("[Texture Hot Reload] Image decode or GPU upload failed: {}", path);
		return result;
	}

	ENGINE_LOG_INFO("[Texture Hot Reload] Successfully reloaded: {} ({}x{})",
		path, m_CachedHeader.width, m_CachedHeader.height);

	return EResult::Success;
}
EResult Texture::ReloadFromEncodedMemory(const vector<uint8>& encodedData, bool generateMips)
{
	const auto result = InitializeFromEncodedMemory(encodedData, generateMips);
	if (result == EResult::Success)
		IncreaseVersion();

	return result;
}
#pragma endregion

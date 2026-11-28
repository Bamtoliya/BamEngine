#pragma once

#include <archive/ArchiveInterface.h>
#include "Texture.h"
#include "Renderer.h"
#include "ResourceUpload.h"
#include "ResourceManager.h"
#include "Runtime.h"
#include "PlatformPath.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "DDSLoader.h"
#include "IBLManager.h"
#include <cctype>
#pragma push_macro("new")
#undef new
#include <archive/BeveArchive.h>
#pragma pop_macro("new")

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

std::string ToLowerAsciiExtension(const std::filesystem::path& path)
{
	std::string ext = path.extension().string();
	for (char& ch : ext)
		ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	return ext;
}

uint32 ComputeHDRCubemapFaceSize(uint32 srcWidth, uint32 srcHeight)
{
	const uint32 raw = (std::max)(srcWidth / 4, srcHeight / 2);
	uint32 pow2 = 16;
	while (pow2 < raw && pow2 < 512)
		pow2 <<= 1;
	return std::clamp(pow2, 16u, 512u);
}

constexpr uint32 TextureArchiveFormatVersion = 1;
[[nodiscard]]
archive::ArchiveResult ValidateTextureHeader(const TextureBinaryHeader& header, f32 ppu, uint32& byteCount)
{
	const bool isValid2D = (header.dimension == ETextureDimension::Texture2D && header.depth == 1 && header.arraySize == 1);
	const bool isValid3D = (header.dimension == ETextureDimension::Texture3D && header.depth >= 1 && header.arraySize == 1);
	const bool isValidCube = (header.dimension == ETextureDimension::TextureCube && header.depth == 1 && header.arraySize == 6);
	if ((!isValid2D && !isValid3D && !isValidCube) || header.mipLevels < 1)
		return { archive::ArchiveErrorCode::UnsupportedType, "Texture.TextureHeader.Layout" };
	const bool isBC = IsBlockCompressed(header.format);
	const bool isSupportedUncompressed = (header.format == ETextureFormat::R8G8B8A8_UNORM ||
		header.format == ETextureFormat::R8G8B8A8_UNORM_SRGB ||
		header.format == ETextureFormat::B8G8R8A8_UNORM ||
		header.format == ETextureFormat::B8G8R8A8_UNORM_SRGB ||
		header.format == ETextureFormat::R16G16_FLOAT ||
		header.format == ETextureFormat::R16G16B16A16_FLOAT ||
		header.format == ETextureFormat::R32_FLOAT ||
		header.format == ETextureFormat::R32G32B32A32_FLOAT);
	if (!isSupportedUncompressed && !isBC)
		return { archive::ArchiveErrorCode::UnsupportedType, "Texture.TextureHeader.format" };
	if (header.width == 0 || header.height == 0 || header.depth == 0)
		return { archive::ArchiveErrorCode::InvalidValue, "Texture.TextureHeader.Size" };
	if (!std::isfinite(ppu) || ppu <= 0.f)
		return { archive::ArchiveErrorCode::InvalidValue, "Texture.PixelPerUnit" };

	uint64 totalBytes = 0;
	if (header.dimension == ETextureDimension::Texture2D || header.dimension == ETextureDimension::TextureCube)
	{
		uint64 sliceBytes = 0;
		uint32 curW = header.width;
		uint32 curH = header.height;
		if (isBC)
		{
			const uint32 blockSize = GetBlockCompressedBlockSize(header.format);
			for (uint32 m = 0; m < header.mipLevels; ++m)
			{
				const uint32 blockW = (curW + 3) / 4;
				const uint32 blockH = (curH + 3) / 4;
				sliceBytes += static_cast<uint64>(blockW) * blockH * blockSize;
				curW = (std::max)(1u, curW >> 1);
				curH = (std::max)(1u, curH >> 1);
			}
		}
		else
		{
			const uint32 bpp = GetUncompressedBytesPerPixel(header.format);
			for (uint32 m = 0; m < header.mipLevels; ++m)
			{
				sliceBytes += static_cast<uint64>(curW) * bpp * curH;
				curW = (std::max)(1u, curW >> 1);
				curH = (std::max)(1u, curH >> 1);
			}
		}
		totalBytes = sliceBytes * header.arraySize;
	}
	else
	{
		const uint32 bpp = GetUncompressedBytesPerPixel(header.format);
		const uint64 sliceBytes = static_cast<uint64>(header.width) * bpp * header.height;
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

bool Texture::ReadImageSize(const std::filesystem::path& path, uint32& width, uint32& height)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) return false;

	stbi_io_callbacks callbacks{};
	callbacks.read = [](void* user, char* data, int size) -> int
		{
			auto& stream = *static_cast<std::ifstream*>(user);
			stream.read(data, size);
			return static_cast<int>(stream.gcount());
		};
	callbacks.skip = [](void* user, int count)
		{
			auto& stream = *static_cast<std::ifstream*>(user);
			stream.clear();
			stream.seekg(count, std::ios::cur);
		};
	callbacks.eof = [](void* user) -> int { return static_cast<std::ifstream*>(user)->eof() ? 1 : 0; };
	int imageWidth = 0;
	int imageHeight = 0;
	int channels = 0;
	if (!stbi_info_from_callbacks(&callbacks, &file, &imageWidth, &imageHeight, &channels) || imageWidth <= 0 || imageHeight <= 0) return false;
	width = static_cast<uint32>(imageWidth);
	height = static_cast<uint32>(imageHeight);
	return true;
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

	if ((desc->dimension != ETextureDimension::Texture2D && desc->dimension != ETextureDimension::TextureCube) ||
		desc->usage != ETextureUsage::Sampler || desc->sRGB)
	{
		return EResult::NotImplemented;
	}


	std::filesystem::path fullPath = PlatformPath::ResolveUri(m_Path);

	vector<uint8> buffer;
	const EResult readResult = PakMountManager::Get().ReadBytesWithFallback(m_Path, buffer);
	if (readResult != EResult::Success)
		return readResult;

	if (buffer.empty() || buffer.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
		return EResult::InvalidArgument;

	const std::string ext = ToLowerAsciiExtension(fullPath);
	if (ext == ".dds" || (buffer.size() >= 4 && std::memcmp(buffer.data(), "DDS ", 4) == 0))
		return InitializeFromDDSMemory(buffer);

	if (ext == ".hdr" || stbi_is_hdr_from_memory(buffer.data(), static_cast<int>(buffer.size())) != 0)
		return InitializeFromHDRMemory(buffer, desc->generateMips);

	return InitializeFromEncodedMemory(buffer, desc->generateMips);
}

EResult Texture::InitializeFromEncodedMemory(const vector<uint8>& encodedData, bool generateMips)
{
	if (encodedData.empty() || encodedData.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
		return EResult::InvalidArgument;

	if (stbi_is_hdr_from_memory(encodedData.data(), static_cast<int>(encodedData.size())) != 0)
		return InitializeFromHDRMemory(encodedData, generateMips);

	RHI* rhi = GetResourceUploadRHI();

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

	RHITexture* texture = nullptr;
	if (rhi)
	{
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

		texture = rhi->CreateTexture(textureDesc);
		if (!texture) return EResult::Fail;
	}

	Safe_Release(m_RHITexture);
	m_RHITexture = texture;
	m_CachedHeader = header;
	m_TempData = std::move(pixelData);
	MarkResident();
	return EResult::Success;
}

EResult Texture::InitializeFromDDSMemory(const vector<uint8>& ddsData)
{
	if (ddsData.empty())
		return EResult::InvalidArgument;

	DDSLoadInfo info{};
	if (!DDSLoader::ParseDDS(ddsData.data(), ddsData.size(), info))
		return EResult::InvalidArgument;

	TextureBinaryHeader header{};
	header.width = info.width;
	header.height = info.height;
	header.depth = info.depth;
	header.mipLevels = info.mipLevels;
	header.arraySize = info.arraySize;
	header.format = info.format;
	header.dimension = info.dimension;
	header.dataSize = static_cast<uint32>(info.pixelDataSize);

	uint32 byteCount = 0;
	if (!ValidateTextureHeader(header, m_PixelPerUnit, byteCount))
		return EResult::InvalidArgument;

	RHI* rhi = GetResourceUploadRHI();
	RHITexture* texture = nullptr;
	if (rhi)
	{
		RHITextureDesc textureDesc{};
		textureDesc.width = header.width;
		textureDesc.height = header.height;
		textureDesc.depth = header.depth;
		textureDesc.mipLevels = header.mipLevels;
		textureDesc.arraySize = header.arraySize;
		textureDesc.format = header.format;
		textureDesc.dimension = header.dimension;
		textureDesc.usage = ETextureUsage::Sampler;
		textureDesc.data = const_cast<uint8*>(info.pixelData);
		textureDesc.dataSize = header.dataSize;

		texture = rhi->CreateTexture(textureDesc);
		if (!texture) return EResult::Fail;
	}

	Safe_Release(m_RHITexture);
	m_RHITexture = texture;
	m_CachedHeader = header;
	m_TempData.assign(info.pixelData, info.pixelData + info.pixelDataSize);
	MarkResident();
	return EResult::Success;
}

Texture* Texture::CreateFromDDSMemory(const vector<uint8>& ddsData, const wstring& key)
{
	Texture* instance = new Texture();
	if (IsFailure(instance->InitializeFromDDSMemory(ddsData)))
	{
		delete instance;
		return nullptr;
	}
	if (!key.empty())
	{
		instance->m_Key = NormalizePath(key);
		instance->m_Path = NormalizePath(key);
	}
	return instance;
}

EResult Texture::InitializeFromHDRMemory(const vector<uint8>& hdrData, bool generateMips)
{
	if (hdrData.empty() || hdrData.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
		return EResult::InvalidArgument;

	const int encodedSize = static_cast<int>(hdrData.size());
	int srcWidth = 0;
	int srcHeight = 0;
	int channels = 0;

	std::unique_ptr<float, decltype(&stbi_image_free)> pixels(
		stbi_loadf_from_memory(hdrData.data(), encodedSize, &srcWidth, &srcHeight, &channels, 3),
		stbi_image_free);

	if (!pixels || srcWidth <= 0 || srcHeight <= 0)
		return EResult::Fail;

	const uint32 faceSize = ComputeHDRCubemapFaceSize(static_cast<uint32>(srcWidth), static_cast<uint32>(srcHeight));
	TextureBinaryHeader header{};
	vector<uint8> cubePixels;
	if (!IBLManager::ConvertEquirectangularToCubemap(pixels.get(), static_cast<uint32>(srcWidth), static_cast<uint32>(srcHeight),
		faceSize, generateMips, header, cubePixels))
		return EResult::Fail;

	uint32 byteCount = 0;
	if (!ValidateTextureHeader(header, m_PixelPerUnit, byteCount) || byteCount != cubePixels.size())
		return EResult::InvalidArgument;

	RHI* rhi = GetResourceUploadRHI();
	RHITexture* texture = nullptr;
	if (rhi)
	{
		RHITextureDesc textureDesc{};
		textureDesc.width = header.width;
		textureDesc.height = header.height;
		textureDesc.depth = header.depth;
		textureDesc.mipLevels = header.mipLevels;
		textureDesc.arraySize = header.arraySize;
		textureDesc.format = header.format;
		textureDesc.dimension = header.dimension;
		textureDesc.usage = ETextureUsage::Sampler;
		textureDesc.data = cubePixels.data();
		textureDesc.dataSize = byteCount;

		texture = rhi->CreateTexture(textureDesc);
		if (!texture) return EResult::Fail;
	}

	Safe_Release(m_RHITexture);
	m_RHITexture = texture;
	m_CachedHeader = header;
	m_TempData = std::move(cubePixels);
	MarkResident();
	return EResult::Success;
}

Texture* Texture::CreateFromHDRMemory(const vector<uint8>& hdrData, const wstring& key, bool generateMips)
{
	std::unique_ptr<Texture> instance(new Texture());
	if (!key.empty())
	{
		instance->m_Key = NormalizePath(key);
		instance->m_Path = NormalizePath(key);
	}
	if (IsFailure(instance->InitializeFromHDRMemory(hdrData, generateMips)))
	{
		instance->Free();
		return nullptr;
	}
	return instance.release();
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

EResult Texture::UploadToGPU()
{
	RHI* rhi = GetResourceUploadRHI();
	if (!rhi || m_RHITexture) return EResult::Success;

	RHITextureDesc textureDesc{};
	textureDesc.width = m_CachedHeader.width;
	textureDesc.height = m_CachedHeader.height;
	textureDesc.depth = m_CachedHeader.depth;
	textureDesc.mipLevels = m_CachedHeader.mipLevels;
	textureDesc.arraySize = m_CachedHeader.arraySize;
	textureDesc.format = m_CachedHeader.format;
	textureDesc.dimension = m_CachedHeader.dimension;
	textureDesc.usage = ETextureUsage::Sampler;
	textureDesc.data = m_TempData.data();
	textureDesc.dataSize = static_cast<uint32>(m_TempData.size());

	m_RHITexture = rhi->CreateTexture(textureDesc);
	return m_RHITexture ? EResult::Success : EResult::Fail;
}

EResult Texture::ApplyStreamedMips(const TextureMipData& mips)
{
	if (!IsPending()) return EResult::Fail;

	if (RHI* rhi = GetResourceUploadRHI())
	{
		RHITextureDesc textureDesc{};
		textureDesc.width = mips.Header.width;
		textureDesc.height = mips.Header.height;
		textureDesc.depth = mips.Header.depth;
		textureDesc.mipLevels = mips.Header.mipLevels;
		textureDesc.arraySize = mips.Header.arraySize;
		textureDesc.format = mips.Header.format;
		textureDesc.dimension = mips.Header.dimension;
		textureDesc.usage = ETextureUsage::Sampler;
		textureDesc.data = const_cast<uint8*>(mips.Pixels.data());
		textureDesc.dataSize = static_cast<uint32>(mips.Pixels.size());
		RHITexture* texture = rhi->CreateTexture(textureDesc);
		if (!texture) return EResult::Fail;

		// 이전 텍스처는 GPU 사용이 끝난 뒤 해제된다.
		Safe_Release(m_RHITexture);
		m_RHITexture = texture;
	}
	m_CachedHeader = mips.Header;
	m_TempData.clear();
	m_TempData.shrink_to_fit();
	IncreaseVersion();
	return EResult::Success;
}

EResult Texture::AdoptPrepared(Resource& prepared)
{
	auto& source = static_cast<Texture&>(prepared);
	const EResult result = source.UploadToGPU();
	if (IsFailure(result)) return result;

	// 임시 텍스처는 prepared와 함께 지연 해제된다.
	std::swap(m_RHITexture, source.m_RHITexture);
	std::swap(m_CachedHeader, source.m_CachedHeader);
	std::swap(m_TempData, source.m_TempData);
	m_PixelPerUnit = source.m_PixelPerUnit;
	return EResult::Success;
}

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
	if (!IsSuccess(const_cast<Texture*>(this)->EnsureResident()))
		return { archive::ArchiveErrorCode::InvalidOperation, "Texture.NotResident" };

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
	RHI* rhi = GetResourceUploadRHI();
	RHITexture* texture = nullptr;
	if (rhi)
	{
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

		texture = rhi->CreateTexture(textureDesc);
		if (!texture)
			return { archive::ArchiveErrorCode::InvalidOperation, "Texture.GPUUpload" };
	}

	m_CachedHeader = textureHeader;
	m_TempData = std::move(pixelData);
	m_PixelPerUnit = pixelPerUnit;
	m_RHITexture = texture;
	MarkResident();
	IncreaseVersion();
	return {};
}
EResult Texture::Reload(const std::filesystem::path& filePath)
{
	std::filesystem::path targetPath = filePath.empty() ? std::filesystem::path(m_Path) : filePath;
	if (targetPath.empty()) return EResult::InvalidArgument;

	targetPath = PlatformPath::ResolveUri(targetPath);

	const auto path = Engine::Unicode::WideToUtf8(targetPath.wstring());

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

	const std::string ext = ToLowerAsciiExtension(targetPath);
	const bool isDDS = (ext == ".dds" || (buffer.size() >= 4 && std::memcmp(buffer.data(), "DDS ", 4) == 0));
	const bool isBamtex = (ext == ".bamtex" || ext == ".beve");
	const bool isHDR = (ext == ".hdr" || stbi_is_hdr_from_memory(buffer.data(), static_cast<int>(buffer.size())) != 0);
	EResult result = EResult::Fail;
	if (isDDS)
	{
		result = InitializeFromDDSMemory(buffer);
		if (result == EResult::Success)
			IncreaseVersion();
	}
	else if (isHDR)
	{
		result = InitializeFromHDRMemory(buffer);
		if (result == EResult::Success)
			IncreaseVersion();
	}
	else if (isBamtex)
	{
		archive::BeveArchive ar;
		const auto loadResult = ar.LoadFromFile(targetPath);
		if (!loadResult)
		{
			ENGINE_LOG_ERROR("[Texture Hot Reload] Archive load failed: path={}, code={}, field={}",
				path, static_cast<int>(loadResult.Code), loadResult.Field);
		}
		else
		{
			Texture replacement;
			const auto archiveResult = replacement.ReadArchive(ar);
			if (!archiveResult)
			{
				ENGINE_LOG_ERROR("[Texture Hot Reload] Archive read failed: path={}, code={}, field={}",
					path, static_cast<int>(archiveResult.Code), archiveResult.Field);
			}
			else
			{
				auto* previous = m_RHITexture;
				m_RHITexture = replacement.m_RHITexture;
				replacement.m_RHITexture = nullptr;
				m_CachedHeader = replacement.m_CachedHeader;
				m_TempData = std::move(replacement.m_TempData);
				m_PixelPerUnit = replacement.m_PixelPerUnit;
				MarkResident();
				IncreaseVersion();
				if (previous) previous->Release();
				result = EResult::Success;
			}
		}
	}
	else
	{
		result = ReloadFromEncodedMemory(buffer);
	}

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

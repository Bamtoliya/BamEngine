#include <archive/BeveArchive.h>
#include <DirectXTex.h>

#include <algorithm>
#include <cwchar>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#include "TextureBinaryHeader.h"
#include "TextureCompressorInterface.h"

int wmain(int argc, wchar_t* argv[])
{
    if (argc != 7)
    {
        std::cerr << "Usage: AssetWorkerTextureOutputCheck <bamtex> <dds> <format> <width> <height> <mips>\n";
        return 2;
    }

    const auto flags = ECompressFlags::GenerateMipMaps | ECompressFlags::SRGB;
    const auto fast = ApplyTextureCompressionMode(flags, ETextureCompressionMode::CpuFast, Engine::ETextureFormat::BC7_RGBA_UNORM);
    const auto gpu = ApplyTextureCompressionMode(flags, ETextureCompressionMode::GpuBC7, Engine::ETextureFormat::BC7_RGBA_UNORM);
    const auto normal = ApplyTextureCompressionMode(flags, ETextureCompressionMode::GpuBC7, Engine::ETextureFormat::BC5_RG_UNORM);
    if (!HasFlag(fast, ECompressFlags::FastBC7) || HasFlag(fast, ECompressFlags::UseGPU) ||
        !HasFlag(gpu, ECompressFlags::UseGPU) || HasFlag(gpu, ECompressFlags::FastBC7) ||
        HasFlag(normal, ECompressFlags::UseGPU) || HasFlag(normal, ECompressFlags::FastBC7))
    {
        std::cerr << "Texture compression mode is not applied correctly.\n";
        return 1;
    }

    archive::BeveArchive archive;
    if (!archive.LoadFromFile(argv[1]))
    {
        std::cerr << "Could not read BAM texture.\n";
        return 1;
    }

    Engine::TextureBinaryHeader header{};
    std::vector<uint8_t> pixels;
    const auto result = archive.ReadObject("Texture", header,
        [&](archive::IArchive& texture, Engine::TextureBinaryHeader& value) -> archive::ArchiveResult
        {
            auto read = texture.ReadObject("TextureHeader", value,
                [](archive::IArchive& fields, Engine::TextureBinaryHeader& h) -> archive::ArchiveResult
                {
                    auto r = fields.Read("width", h.width);
                    if (r) r = fields.Read("height", h.height);
                    if (r) r = fields.Read("depth", h.depth);
                    if (r) r = fields.Read("mipLevels", h.mipLevels);
                    if (r) r = fields.Read("arraySize", h.arraySize);
                    if (r) r = fields.Read("format", h.format);
                    if (r) r = fields.Read("dimension", h.dimension);
                    if (r) r = fields.Read("dataSize", h.dataSize);
                    return r;
                });
            if (read) read = texture.ReadArray("PixelData", pixels);
            return read;
        });
    if (!result)
    {
        std::cerr << "Could not read BAM texture header and pixels.\n";
        return 1;
    }

    DXGI_FORMAT expectedFormat = DXGI_FORMAT_UNKNOWN;
    switch (header.format)
    {
    case Engine::ETextureFormat::R8G8B8A8_UNORM: expectedFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
    case Engine::ETextureFormat::R8G8B8A8_UNORM_SRGB: expectedFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; break;
    case Engine::ETextureFormat::BC5_RG_UNORM: expectedFormat = DXGI_FORMAT_BC5_UNORM; break;
    case Engine::ETextureFormat::BC7_RGBA_UNORM_SRGB: expectedFormat = DXGI_FORMAT_BC7_UNORM_SRGB; break;
    default: break;
    }
    const std::wstring_view formatName(argv[3]);
    const DXGI_FORMAT requestedFormat = formatName == L"BC5" ? DXGI_FORMAT_BC5_UNORM
        : formatName == L"BC7SRGB" ? DXGI_FORMAT_BC7_UNORM_SRGB
        : formatName == L"RGBA" ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_UNKNOWN;

    DirectX::ScratchImage image;
    DirectX::TexMetadata metadata{};
    if (expectedFormat == DXGI_FORMAT_UNKNOWN || expectedFormat != requestedFormat ||
        FAILED(DirectX::LoadFromDDSFile(argv[2], DirectX::DDS_FLAGS_NONE, &metadata, image)) ||
        metadata.format != expectedFormat || metadata.width != header.width || metadata.height != header.height ||
        metadata.depth != header.depth || metadata.arraySize != header.arraySize ||
        metadata.mipLevels != header.mipLevels || header.dimension != Engine::ETextureDimension::Texture2D ||
        metadata.width != std::wcstoul(argv[4], nullptr, 10) ||
        metadata.height != std::wcstoul(argv[5], nullptr, 10) ||
        metadata.mipLevels != std::wcstoul(argv[6], nullptr, 10) ||
        pixels.size() != header.dataSize || pixels.size() != image.GetPixelsSize() ||
        !std::equal(pixels.begin(), pixels.end(), image.GetPixels()))
    {
        std::cerr << "BAM texture and DDS metadata or pixels differ.\n";
        return 1;
    }

    std::unique_ptr<Tools::TextureCompressorInterface> compressor(Tools::TextureCompressorFactory::Create());
    const std::filesystem::path invalidPath = std::wstring(argv[2]) + L".invalid";
    auto invalidHeader = header;
    invalidHeader.arraySize = 2;
    if (!compressor || compressor->SaveCompressedDataToDDS(invalidPath.wstring(), pixels, invalidHeader) ||
        compressor->SaveCompressedDataToDDS(invalidPath.wstring(), {}, header) || std::filesystem::exists(invalidPath))
    {
        std::cerr << "Invalid texture data must be rejected without creating a DDS file.\n";
        return 1;
    }

    return 0;
}

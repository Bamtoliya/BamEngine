#include "DDSLoader.h"
#include <cstring>
#include <algorithm>

namespace Engine
{
    namespace
    {
        constexpr uint32 DDS_MAGIC = 0x20534444; // "DDS "

        struct DDS_PIXELFORMAT
        {
            uint32 dwSize;
            uint32 dwFlags;
            uint32 dwFourCC;
            uint32 dwRGBBitCount;
            uint32 dwRBitMask;
            uint32 dwGBitMask;
            uint32 dwBBitMask;
            uint32 dwABitMask;
        };

        struct DDS_HEADER
        {
            uint32 dwSize;
            uint32 dwFlags;
            uint32 dwHeight;
            uint32 dwWidth;
            uint32 dwPitchOrLinearSize;
            uint32 dwDepth;
            uint32 dwMipMapCount;
            uint32 dwReserved1[11];
            DDS_PIXELFORMAT ddspf;
            uint32 dwCaps;
            uint32 dwCaps2;
            uint32 dwCaps3;
            uint32 dwCaps4;
            uint32 dwReserved2;
        };

        struct DDS_HEADER_DXT10
        {
            uint32 dxgiFormat;
            uint32 resourceDimension;
            uint32 miscFlag;
            uint32 arraySize;
            uint32 miscFlags2;
        };

        constexpr uint32 DDPF_ALPHAPIXELS = 0x00000001;
        constexpr uint32 DDPF_FOURCC      = 0x00000004;
        constexpr uint32 DDPF_RGB         = 0x00000040;
        constexpr uint32 DDSCAPS2_CUBEMAP = 0x00000200;
        constexpr uint32 D3D10_RESOURCE_MISC_TEXTURECUBE = 0x00000004;

        constexpr uint32 MAKE_FOURCC(char a, char b, char c, char d)
        {
            return (static_cast<uint32>(static_cast<uint8>(a))) |
                   (static_cast<uint32>(static_cast<uint8>(b)) << 8) |
                   (static_cast<uint32>(static_cast<uint8>(c)) << 16) |
                   (static_cast<uint32>(static_cast<uint8>(d)) << 24);
        }

        ETextureFormat DxgiToTextureFormat(uint32 dxgi)
        {
            switch (dxgi)
            {
            case 28: return ETextureFormat::R8G8B8A8_UNORM;       // DXGI_FORMAT_R8G8B8A8_UNORM
            case 29: return ETextureFormat::R8G8B8A8_UNORM_SRGB;  // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            case 87: return ETextureFormat::B8G8R8A8_UNORM;       // DXGI_FORMAT_B8G8R8A8_UNORM
            case 91: return ETextureFormat::B8G8R8A8_UNORM_SRGB;  // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
            case 10: return ETextureFormat::R16G16B16A16_FLOAT;   // DXGI_FORMAT_R16G16B16A16_FLOAT
            case 26: return ETextureFormat::R11G11B10_UFLOAT;     // DXGI_FORMAT_R11G11B10_FLOAT
            case 2:  return ETextureFormat::R32G32B32A32_FLOAT;   // DXGI_FORMAT_R32G32B32A32_FLOAT
            case 71: return ETextureFormat::BC1_RGBA_UNORM;       // DXGI_FORMAT_BC1_UNORM
            case 72: return ETextureFormat::BC1_RGBA_UNORM_SRGB;  // DXGI_FORMAT_BC1_UNORM_SRGB
            case 74: return ETextureFormat::BC2_RGBA_UNORM;       // DXGI_FORMAT_BC2_UNORM
            case 75: return ETextureFormat::BC2_RGBA_UNORM_SRGB;  // DXGI_FORMAT_BC2_UNORM_SRGB
            case 77: return ETextureFormat::BC3_RGBA_UNORM;       // DXGI_FORMAT_BC3_UNORM
            case 78: return ETextureFormat::BC3_RGBA_UNORM_SRGB;  // DXGI_FORMAT_BC3_UNORM_SRGB
            case 80: return ETextureFormat::BC4_R_UNORM;          // DXGI_FORMAT_BC4_UNORM
            case 83: return ETextureFormat::BC5_RG_UNORM;         // DXGI_FORMAT_BC5_UNORM
            case 95: return ETextureFormat::BC6H_RGB_UFLOAT;      // DXGI_FORMAT_BC6H_UF16
            case 96: return ETextureFormat::BC6H_RGB_FLOAT;       // DXGI_FORMAT_BC6H_SF16
            case 98: return ETextureFormat::BC7_RGBA_UNORM;       // DXGI_FORMAT_BC7_UNORM
            case 99: return ETextureFormat::BC7_RGBA_UNORM_SRGB;  // DXGI_FORMAT_BC7_UNORM_SRGB
            default: return ETextureFormat::UNKNOWN;
            }
        }
    }

    bool DDSLoader::ParseDDS(const uint8* data, size_t dataSize, DDSLoadInfo& outInfo)
    {
        if (!data || dataSize < (sizeof(uint32) + sizeof(DDS_HEADER)))
            return false;

        uint32 magic = 0;
        std::memcpy(&magic, data, sizeof(uint32));
        if (magic != DDS_MAGIC)
            return false;

        DDS_HEADER header{};
        std::memcpy(&header, data + sizeof(uint32), sizeof(DDS_HEADER));
        if (header.dwSize != sizeof(DDS_HEADER) || header.ddspf.dwSize != sizeof(DDS_PIXELFORMAT))
            return false;

        size_t headerSize = sizeof(uint32) + sizeof(DDS_HEADER);

        outInfo.width = header.dwWidth;
        outInfo.height = header.dwHeight;
        outInfo.depth = (header.dwDepth > 0) ? header.dwDepth : 1;
        outInfo.mipLevels = (header.dwMipMapCount > 0) ? header.dwMipMapCount : 1;
        outInfo.arraySize = 1;
        if ((header.dwCaps2 & DDSCAPS2_CUBEMAP) != 0)
        {
            outInfo.dimension = ETextureDimension::TextureCube;
            outInfo.arraySize = 6;
            outInfo.depth = 1;
        }
        else
        {
            outInfo.dimension = (header.dwDepth > 1) ? ETextureDimension::Texture3D : ETextureDimension::Texture2D;
        }

        if (header.ddspf.dwFlags & DDPF_FOURCC)
        {
            if (header.ddspf.dwFourCC == MAKE_FOURCC('D', 'X', '1', '0'))
            {
                if (dataSize < headerSize + sizeof(DDS_HEADER_DXT10))
                    return false;

                DDS_HEADER_DXT10 dxt10{};
                std::memcpy(&dxt10, data + headerSize, sizeof(DDS_HEADER_DXT10));
                headerSize += sizeof(DDS_HEADER_DXT10);

                outInfo.format = DxgiToTextureFormat(dxt10.dxgiFormat);
                const uint32 dxt10Array = (dxt10.arraySize > 0) ? dxt10.arraySize : 1;
                if ((dxt10.miscFlag & D3D10_RESOURCE_MISC_TEXTURECUBE) != 0)
                {
                    outInfo.dimension = (dxt10Array > 1) ? ETextureDimension::TextureCubeArray : ETextureDimension::TextureCube;
                    outInfo.arraySize = dxt10Array * 6;
                    outInfo.depth = 1;
                }
                else
                {
                    outInfo.arraySize = dxt10Array;
                }
            }
            else if (header.ddspf.dwFourCC == MAKE_FOURCC('D', 'X', 'T', '1'))
            {
                outInfo.format = ETextureFormat::BC1_RGBA_UNORM;
            }
            else if (header.ddspf.dwFourCC == MAKE_FOURCC('D', 'X', 'T', '3'))
            {
                outInfo.format = ETextureFormat::BC2_RGBA_UNORM;
            }
            else if (header.ddspf.dwFourCC == MAKE_FOURCC('D', 'X', 'T', '5'))
            {
                outInfo.format = ETextureFormat::BC3_RGBA_UNORM;
            }
            else if (header.ddspf.dwFourCC == MAKE_FOURCC('A', 'T', 'I', '1') ||
                     header.ddspf.dwFourCC == MAKE_FOURCC('B', 'C', '4', 'U'))
            {
                outInfo.format = ETextureFormat::BC4_R_UNORM;
            }
            else if (header.ddspf.dwFourCC == MAKE_FOURCC('A', 'T', 'I', '2') ||
                     header.ddspf.dwFourCC == MAKE_FOURCC('B', 'C', '5', 'U'))
            {
                outInfo.format = ETextureFormat::BC5_RG_UNORM;
            }
            else if (header.ddspf.dwFourCC == 111) // D3DFMT_R16F
            {
                outInfo.format = ETextureFormat::R16_FLOAT;
            }
            else if (header.ddspf.dwFourCC == 112) // D3DFMT_G16R16F
            {
                outInfo.format = ETextureFormat::R16G16_FLOAT;
            }
            else if (header.ddspf.dwFourCC == 113) // D3DFMT_A16B16G16R16F
            {
                outInfo.format = ETextureFormat::R16G16B16A16_FLOAT;
            }
            else if (header.ddspf.dwFourCC == 114) // D3DFMT_R32F
            {
                outInfo.format = ETextureFormat::R32_FLOAT;
            }
            else if (header.ddspf.dwFourCC == 115) // D3DFMT_G32R32F
            {
                outInfo.format = ETextureFormat::R32G32_FLOAT;
            }
            else if (header.ddspf.dwFourCC == 116) // D3DFMT_A32B32G32R32F
            {
                outInfo.format = ETextureFormat::R32G32B32A32_FLOAT;
            }
            else
            {
                return false;
            }
        }
        else if (header.ddspf.dwFlags & DDPF_RGB)
        {
            if (header.ddspf.dwRGBBitCount == 32)
            {
                if (header.ddspf.dwRBitMask == 0x000000ff &&
                    header.ddspf.dwGBitMask == 0x0000ff00 &&
                    header.ddspf.dwBBitMask == 0x00ff0000)
                {
                    outInfo.format = ETextureFormat::R8G8B8A8_UNORM;
                }
                else if (header.ddspf.dwRBitMask == 0x00ff0000 &&
                         header.ddspf.dwGBitMask == 0x0000ff00 &&
                         header.ddspf.dwBBitMask == 0x000000ff)
                {
                    outInfo.format = ETextureFormat::B8G8R8A8_UNORM;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }

        if (outInfo.format == ETextureFormat::UNKNOWN)
            return false;

        if (dataSize < headerSize)
            return false;

        outInfo.pixelData = data + headerSize;
        outInfo.pixelDataSize = dataSize - headerSize;
        return true;
    }
}

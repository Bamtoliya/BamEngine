#include "TexturePlaceholderCache.h"
#include "PlatformPath.h"
#include "Texture.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <fstream>

namespace Engine
{
    namespace TexturePlaceholderCacheDetail
    {
        constexpr uint32 PlaceholderMagic = 0x48505442; // 'BTPH'
        constexpr uint32 PlaceholderVersion = 1;

        struct PlaceholderFileHeader
        {
            uint32 Magic = PlaceholderMagic;
            uint32 Version = PlaceholderVersion;
            uint64 SourceSize = 0;
            int64 SourceTime = 0;
            uint32 Width = 0;
            uint32 Height = 0;
            uint32 MipLevels = 0;
            uint32 DataSize = 0;
            uint32 KeyBytes = 0;
            uint32 Reserved = 0;
        };
        static_assert(sizeof(PlaceholderFileHeader) == 48);

        static bool ReadPlaceholderSourceStamp(const std::filesystem::path& source, uint64& size, int64& time)
        {
            std::error_code error;
            size = std::filesystem::file_size(source, error);
            if (error) return false;
            time = std::filesystem::last_write_time(source, error).time_since_epoch().count();
            return !error;
        }

        static std::filesystem::path GetPlaceholderPath(const std::string& key)
        {
            uint64 hash = 14695981039346656037ull;
            for (const char c : key)
            {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ull;
            }
            char name[32];
            std::snprintf(name, sizeof(name), "%016llx.btph", static_cast<unsigned long long>(hash));
            return PlatformPath::GetProjectRoot() / ".bam" / "cache" / "textures" / name;
        }

        // 1x1까지 이어지지 않는 밉도 있으므로 개수만큼 더한다.
        static uint64 GetPlaceholderMipBytes(uint32 width, uint32 height, uint32 mipLevels)
        {
            uint64 bytes = 0;
            for (uint32 i = 0; i < mipLevels; ++i)
                bytes += static_cast<uint64>((std::max)(1u, width >> i)) * (std::max)(1u, height >> i) * 4;
            return bytes;
        }

        static bool IsPlaceholderLayout(uint32 width, uint32 height, uint32 mipLevels)
        {
            return width > 0 && height > 0 && width <= TexturePlaceholderCache::MaxSize && height <= TexturePlaceholderCache::MaxSize &&
                mipLevels > 0 && mipLevels <= Texture::CalculateMipLevels(width, height);
        }

        static TextureBinaryHeader MakePlaceholderHeader(uint32 width, uint32 height, uint32 mipLevels, uint32 dataSize)
        {
            TextureBinaryHeader header{};
            header.width = width;
            header.height = height;
            header.depth = 1;
            header.mipLevels = mipLevels;
            header.arraySize = 1;
            header.format = ETextureFormat::R8G8B8A8_UNORM;
            header.dimension = ETextureDimension::Texture2D;
            header.dataSize = dataSize;
            return header;
        }
    }

    using namespace TexturePlaceholderCacheDetail;

    std::optional<TexturePlaceholder> TexturePlaceholderCache::Load(const std::wstring& key, const std::filesystem::path& source)
    {
        const std::string keyText = WStrToStr(key);
        uint64 size = 0;
        int64 time = 0;
        if (!ReadPlaceholderSourceStamp(source, size, time)) return std::nullopt;

        std::ifstream file(GetPlaceholderPath(keyText), std::ios::binary);
        PlaceholderFileHeader header;
        if (!file || !file.read(reinterpret_cast<char*>(&header), sizeof(header))) return std::nullopt;
        if (header.Magic != PlaceholderMagic || header.Version != PlaceholderVersion || header.SourceSize != size || header.SourceTime != time ||
            header.KeyBytes != keyText.size() || !IsPlaceholderLayout(header.Width, header.Height, header.MipLevels) ||
            header.DataSize != GetPlaceholderMipBytes(header.Width, header.Height, header.MipLevels)) return std::nullopt;

        std::string storedKey(header.KeyBytes, '\0');
        TexturePlaceholder result;
        result.Pixels.resize(header.DataSize);
        if (!file.read(storedKey.data(), static_cast<std::streamsize>(storedKey.size())) || storedKey != keyText ||
            !file.read(reinterpret_cast<char*>(result.Pixels.data()), static_cast<std::streamsize>(result.Pixels.size()))) return std::nullopt;
        result.Header = MakePlaceholderHeader(header.Width, header.Height, header.MipLevels, header.DataSize);
        return result;
    }

    bool TexturePlaceholderCache::Store(const std::wstring& key, const std::filesystem::path& source, const TexturePlaceholder& placeholder)
    {
        const auto& layout = placeholder.Header;
        if (layout.format != ETextureFormat::R8G8B8A8_UNORM || !IsPlaceholderLayout(layout.width, layout.height, layout.mipLevels) ||
            placeholder.Pixels.size() != GetPlaceholderMipBytes(layout.width, layout.height, layout.mipLevels)) return false;

        const std::string keyText = WStrToStr(key);
        PlaceholderFileHeader header;
        if (!ReadPlaceholderSourceStamp(source, header.SourceSize, header.SourceTime)) return false;
        header.Width = layout.width;
        header.Height = layout.height;
        header.MipLevels = layout.mipLevels;
        header.DataSize = static_cast<uint32>(placeholder.Pixels.size());
        header.KeyBytes = static_cast<uint32>(keyText.size());

        // 쓰다 만 파일이 남지 않도록 다른 이름으로 쓰고 바꾼다.
        static std::atomic<uint32> s_TemporaryIndex = 0;
        const auto path = GetPlaceholderPath(keyText);
        auto temporary = path;
        temporary += "." + std::to_string(s_TemporaryIndex++) + ".tmp";
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        bool written = false;
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            file.write(keyText.data(), static_cast<std::streamsize>(keyText.size()));
            file.write(reinterpret_cast<const char*>(placeholder.Pixels.data()), static_cast<std::streamsize>(placeholder.Pixels.size()));
            file.close();
            written = file.good();
        }
        if (written) std::filesystem::rename(temporary, path, error);
        if (written && !error) return true;
        std::filesystem::remove(temporary, error);
        return false;
    }

    std::optional<TexturePlaceholder> TexturePlaceholderCache::Extract(const TextureBinaryHeader& header, const std::vector<uint8>& pixels)
    {
        if (header.format != ETextureFormat::R8G8B8A8_UNORM || header.dimension != ETextureDimension::Texture2D || header.arraySize != 1 ||
            header.width == 0 || header.height == 0 || header.mipLevels == 0 || header.mipLevels > Texture::CalculateMipLevels(header.width, header.height) ||
            pixels.size() != GetPlaceholderMipBytes(header.width, header.height, header.mipLevels)) return std::nullopt;

        uint32 mip = 0;
        while (mip < header.mipLevels && (std::max)(header.width >> mip, header.height >> mip) > MaxSize) ++mip;
        if (mip == header.mipLevels) return std::nullopt;

        const uint32 width = (std::max)(1u, header.width >> mip);
        const uint32 height = (std::max)(1u, header.height >> mip);
        const auto offset = static_cast<std::ptrdiff_t>(GetPlaceholderMipBytes(header.width, header.height, mip));
        TexturePlaceholder result;
        result.Pixels.assign(pixels.begin() + offset, pixels.end());
        result.Header = MakePlaceholderHeader(width, height, header.mipLevels - mip, static_cast<uint32>(result.Pixels.size()));
        return result;
    }

    TexturePlaceholder TexturePlaceholderCache::MakeSolid(uint32 materialSlot)
    {
        std::array<uint8, 4> color{ 128, 128, 128, 255 };
        switch (materialSlot)
        {
        case 1: color = { 128, 128, 255, 255 }; break;  // 평평한 법선
        case 2: color = { 160, 0, 255, 255 }; break;    // 거칠기, 금속, 캐비티, AO
        case 3: color = { 0, 0, 0, 255 }; break;        // 발광 없음
        default: break;
        }
        TexturePlaceholder result;
        result.Pixels.assign(color.begin(), color.end());
        result.Header = MakePlaceholderHeader(1, 1, 1, 4);
        return result;
    }
}

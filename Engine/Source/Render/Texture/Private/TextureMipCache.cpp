#include "TextureMipCache.h"
#include "PlatformPath.h"
#include "Texture.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <fstream>

namespace Engine
{
    namespace TextureMipCacheDetail
    {
        constexpr uint32 MipCacheMagic = 0x504D5442; // 'BTMP'
        constexpr uint32 MipCacheVersion = 1;
        constexpr uint32 MaxMipCacheDimension = 16384;

        struct MipCacheFileHeader
        {
            uint32 Magic = MipCacheMagic;
            uint32 Version = MipCacheVersion;
            uint64 SourceSize = 0;
            int64 SourceTime = 0;
            uint32 Width = 0;
            uint32 Height = 0;
            uint32 MipLevels = 0;
            uint32 KeyBytes = 0;
        };
        static_assert(sizeof(MipCacheFileHeader) == 40);

        static bool ReadMipCacheSourceStamp(const std::filesystem::path& source, uint64& size, int64& time)
        {
            std::error_code error;
            size = std::filesystem::file_size(source, error);
            if (error) return false;
            time = std::filesystem::last_write_time(source, error).time_since_epoch().count();
            return !error;
        }

        static std::filesystem::path GetMipCachePath(const std::string& key)
        {
            uint64 hash = 14695981039346656037ull;
            for (const char c : key)
            {
                hash ^= static_cast<unsigned char>(c);
                hash *= 1099511628211ull;
            }
            char name[32];
            std::snprintf(name, sizeof(name), "%016llx.btmip", static_cast<unsigned long long>(hash));
            return PlatformPath::GetProjectRoot() / ".bam" / "cache" / "textures" / name;
        }

        static TextureBinaryHeader MakeMipHeader(const TextureMipLayout& layout, uint32 firstMip)
        {
            TextureBinaryHeader header{};
            header.width = layout.GetWidth(firstMip);
            header.height = layout.GetHeight(firstMip);
            header.depth = 1;
            header.mipLevels = layout.MipLevels - firstMip;
            header.arraySize = 1;
            header.format = ETextureFormat::R8G8B8A8_UNORM;
            header.dimension = ETextureDimension::Texture2D;
            header.dataSize = static_cast<uint32>(layout.GetTailBytes(firstMip));
            return header;
        }

        static std::optional<TextureMipLayout> GetMipLayout(const TextureBinaryHeader& header, const std::vector<uint8>& pixels)
        {
            const TextureMipLayout layout{ header.width, header.height, header.mipLevels };
            if (header.format != ETextureFormat::R8G8B8A8_UNORM || header.dimension != ETextureDimension::Texture2D || header.arraySize != 1 ||
                !layout.IsValid() || pixels.size() != layout.GetTailBytes(0)) return std::nullopt;
            return layout;
        }

        // 머리글과 키를 확인하고 픽셀 바로 앞에 둔다.
        static std::optional<TextureMipLayout> OpenMipCache(const std::wstring& key, const std::filesystem::path& source, std::ifstream& file)
        {
            const std::string keyText = WStrToStr(key);
            uint64 size = 0;
            int64 time = 0;
            if (!ReadMipCacheSourceStamp(source, size, time)) return std::nullopt;

            file.open(GetMipCachePath(keyText), std::ios::binary);
            MipCacheFileHeader header;
            if (!file || !file.read(reinterpret_cast<char*>(&header), sizeof(header))) return std::nullopt;
            const TextureMipLayout layout{ header.Width, header.Height, header.MipLevels };
            if (header.Magic != MipCacheMagic || header.Version != MipCacheVersion || header.SourceSize != size || header.SourceTime != time ||
                header.KeyBytes != keyText.size() || !layout.IsValid()) return std::nullopt;

            std::string storedKey(header.KeyBytes, '\0');
            if (!file.read(storedKey.data(), static_cast<std::streamsize>(storedKey.size())) || storedKey != keyText) return std::nullopt;
            return layout;
        }
    }

    using namespace TextureMipCacheDetail;

    bool TextureMipLayout::IsValid() const
    {
        return Width > 0 && Height > 0 && Width <= MaxMipCacheDimension && Height <= MaxMipCacheDimension &&
            MipLevels > 0 && MipLevels <= Texture::CalculateMipLevels(Width, Height);
    }

    uint64 TextureMipLayout::GetTailBytes(uint32 firstMip) const
    {
        uint64 bytes = 0;
        for (uint32 mip = firstMip; mip < MipLevels; ++mip)
            bytes += static_cast<uint64>(GetWidth(mip)) * GetHeight(mip) * 4;
        return bytes;
    }

    uint32 TextureMipLayout::FindMipForSize(uint32 size) const
    {
        uint32 mip = 0;
        while (mip + 1 < MipLevels && (std::max)(GetWidth(mip), GetHeight(mip)) > size) ++mip;
        return mip;
    }

    std::optional<TextureMipLayout> TextureMipCache::ReadLayout(const std::wstring& key, const std::filesystem::path& source)
    {
        std::ifstream file;
        return OpenMipCache(key, source, file);
    }

    std::optional<TextureMipData> TextureMipCache::Load(const std::wstring& key, const std::filesystem::path& source, uint32 firstMip)
    {
        std::ifstream file;
        const auto layout = OpenMipCache(key, source, file);
        if (!layout || firstMip >= layout->MipLevels) return std::nullopt;

        TextureMipData result;
        result.Pixels.resize(static_cast<size_t>(layout->GetTailBytes(firstMip)));
        for (uint32 mip = layout->MipLevels; mip-- > firstMip;)
        {
            const uint64 bytes = layout->GetTailBytes(mip) - layout->GetTailBytes(mip + 1);
            const uint64 offset = result.Pixels.size() - layout->GetTailBytes(mip);
            if (!file.read(reinterpret_cast<char*>(result.Pixels.data() + offset), static_cast<std::streamsize>(bytes))) return std::nullopt;
        }
        result.Header = MakeMipHeader(*layout, firstMip);
        return result;
    }

    bool TextureMipCache::Store(const std::wstring& key, const std::filesystem::path& source, const TextureBinaryHeader& sourceHeader, const std::vector<uint8>& pixels)
    {
        const auto layout = GetMipLayout(sourceHeader, pixels);
        if (!layout) return false;

        const std::string keyText = WStrToStr(key);
        MipCacheFileHeader header;
        if (!ReadMipCacheSourceStamp(source, header.SourceSize, header.SourceTime)) return false;
        header.Width = layout->Width;
        header.Height = layout->Height;
        header.MipLevels = layout->MipLevels;
        header.KeyBytes = static_cast<uint32>(keyText.size());

        // 쓰다 만 파일이 남지 않도록 다른 이름으로 쓰고 바꾼다.
        static std::atomic<uint32> s_TemporaryIndex = 0;
        const auto path = GetMipCachePath(keyText);
        auto temporary = path;
        temporary += "." + std::to_string(s_TemporaryIndex++) + ".tmp";
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        bool written = false;
        {
            std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
            file.write(reinterpret_cast<const char*>(&header), sizeof(header));
            file.write(keyText.data(), static_cast<std::streamsize>(keyText.size()));
            for (uint32 mip = layout->MipLevels; mip-- > 0;)
            {
                const uint64 bytes = layout->GetTailBytes(mip) - layout->GetTailBytes(mip + 1);
                const uint64 offset = pixels.size() - layout->GetTailBytes(mip);
                file.write(reinterpret_cast<const char*>(pixels.data() + offset), static_cast<std::streamsize>(bytes));
            }
            file.close();
            written = file.good();
        }
        if (written) std::filesystem::rename(temporary, path, error);
        if (written && !error) return true;
        std::filesystem::remove(temporary, error);
        return false;
    }

    std::optional<TextureMipData> TextureMipCache::Slice(const TextureBinaryHeader& header, const std::vector<uint8>& pixels, uint32 firstMip)
    {
        const auto layout = GetMipLayout(header, pixels);
        if (!layout || firstMip >= layout->MipLevels) return std::nullopt;

        TextureMipData result;
        result.Pixels.assign(pixels.end() - static_cast<std::ptrdiff_t>(layout->GetTailBytes(firstMip)), pixels.end());
        result.Header = MakeMipHeader(*layout, firstMip);
        return result;
    }

    TextureMipData TextureMipCache::MakeSolid(uint32 materialSlot)
    {
        std::array<uint8, 4> color{ 128, 128, 128, 255 };
        switch (materialSlot)
        {
        case 1: color = { 128, 128, 255, 255 }; break;  // 평평한 법선
        case 2: color = { 160, 0, 255, 255 }; break;    // 거칠기, 금속, 캐비티, AO
        case 3: color = { 0, 0, 0, 255 }; break;        // 발광 없음
        default: break;
        }
        TextureMipData result;
        result.Pixels.assign(color.begin(), color.end());
        result.Header = MakeMipHeader({ 1, 1, 1 }, 0);
        return result;
    }
}

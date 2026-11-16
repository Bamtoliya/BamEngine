#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace Tools::Ddc
{
    // FNV-1a 64비트 스트림 해시 누적기
    struct DdcHasher
    {
        uint64_t Hash = 14695981039346656037ULL;

        void Combine(const void* data, size_t size)
        {
            const auto* bytes = static_cast<const uint8_t*>(data);
            for (size_t i = 0; i < size; ++i)
            {
                Hash ^= bytes[i];
                Hash *= 1099511628211ULL;
            }
        }

        template<typename T>
        void CombinePod(const T& val) { Combine(&val, sizeof(T)); }

        void CombineString(std::string_view s) { Combine(s.data(), s.size()); }

        void CombineFileMetadata(const std::filesystem::path& path)
        {
            std::error_code ec;
            const auto time = std::filesystem::last_write_time(path, ec).time_since_epoch().count();
            if (!ec) CombinePod(time);
            const auto size = std::filesystem::file_size(path, ec);
            if (!ec) CombinePod(size);
        }

        std::filesystem::path ResolvePath(const std::filesystem::path& dir, std::wstring_view ext = L".ddc") const
        {
            wchar_t buf[32];
            std::swprintf(buf, sizeof(buf) / sizeof(wchar_t), L"%016llx%ls", static_cast<unsigned long long>(Hash), ext.data());
            return dir / buf;
        }
    };

    // 프로젝트 루트를 역추적하여 .bam/cache/ddc/<subCategory> 디렉터리 반환
    inline std::filesystem::path FindDdcDirectory(const char* subCategory)
    {
        std::error_code ec;
        std::filesystem::path cur = std::filesystem::current_path(ec);
        for (int i = 0; i < 7; ++i)
        {
            if (std::filesystem::exists(cur / "Config" / "Project.json", ec) ||
                std::filesystem::exists(cur / "CMakeLists.txt", ec))
            {
                const auto ddcDir = cur / ".bam" / "cache" / "ddc" / subCategory;
                std::filesystem::create_directories(ddcDir, ec);
                return ddcDir;
            }
            if (!cur.has_parent_path()) break;
            cur = cur.parent_path();
        }
        const auto fallback = std::filesystem::current_path(ec) / ".bam" / "cache" / "ddc" / subCategory;
        std::filesystem::create_directories(fallback, ec);
        return fallback;
    }
}
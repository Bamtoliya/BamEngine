#pragma once

#include "Engine_API.h"
#include "Macro.h"
#include "Result.h"
#include "Types.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <span>
#include <vector>
#include <cctype>

BEGIN(Engine)

// -----------------------------------------------------------------------------
// 포맷 상수 및 안전 한도
// -----------------------------------------------------------------------------
inline constexpr uint32 PAK_MAGIC = 0x4B415042; // "BPAK" ('B' | ('P' << 8) | ('A' << 16) | ('K' << 24))
inline constexpr uint32 PAK_FORMAT_VERSION = 1;

inline constexpr uint32 PAK_MAX_ENTRIES = 1'000'000;
inline constexpr uint32 PAK_MAX_URI_LENGTH = 1024;
inline constexpr uint64 PAK_MAX_ENTRY_BYTES = 2ULL * 1024 * 1024 * 1024; // 2 GB
inline constexpr uint64 PAK_MAX_FILE_SIZE = 64ULL * 1024 * 1024 * 1024;  // 64 GB
inline constexpr size_t PAK_HEADER_SIZE = 48;

// -----------------------------------------------------------------------------
// 열거형 정의
// -----------------------------------------------------------------------------
enum class EPakPayloadFormat : uint16
{
    Raw = 0,
    Json = 1,
    Beve = 2,
    Dxil = 3,
    Dds = 4,
};

enum class EPakCompression : uint16
{
    None = 0,
};

// -----------------------------------------------------------------------------
// 리틀 엔디안 명시적 직렬화 / 역직렬화 (구조체 패딩 및 아키텍처 비의존)
// -----------------------------------------------------------------------------
inline void WriteUint16LE(uint8* dest, uint16 value) noexcept
{
    dest[0] = static_cast<uint8>(value & 0xFF);
    dest[1] = static_cast<uint8>((value >> 8) & 0xFF);
}

inline void WriteUint32LE(uint8* dest, uint32 value) noexcept
{
    dest[0] = static_cast<uint8>(value & 0xFF);
    dest[1] = static_cast<uint8>((value >> 8) & 0xFF);
    dest[2] = static_cast<uint8>((value >> 16) & 0xFF);
    dest[3] = static_cast<uint8>((value >> 24) & 0xFF);
}

inline void WriteUint64LE(uint8* dest, uint64 value) noexcept
{
    for (int i = 0; i < 8; ++i)
    {
        dest[i] = static_cast<uint8>((value >> (i * 8)) & 0xFF);
    }
}

inline uint16 ReadUint16LE(const uint8* src) noexcept
{
    return static_cast<uint16>(src[0]) |
          (static_cast<uint16>(src[1]) << 8);
}

inline uint32 ReadUint32LE(const uint8* src) noexcept
{
    return static_cast<uint32>(src[0]) |
          (static_cast<uint32>(src[1]) << 8) |
          (static_cast<uint32>(src[2]) << 16) |
          (static_cast<uint32>(src[3]) << 24);
}

inline uint64 ReadUint64LE(const uint8* src) noexcept
{
    uint64 val = 0;
    for (int i = 0; i < 8; ++i)
    {
        val |= (static_cast<uint64>(src[i]) << (i * 8));
    }
    return val;
}

// -----------------------------------------------------------------------------
// FNV-1a 64-bit 무결성 해시
// -----------------------------------------------------------------------------
inline uint64 ComputePakHash64(const void* data, size_t size) noexcept
{
    constexpr uint64 FNV_OFFSET_BASIS = 14695981039346656037ULL; // 0xcbf29ce484222325
    constexpr uint64 FNV_PRIME = 1099511628211ULL;              // 0x100000001b3

    uint64 hash = FNV_OFFSET_BASIS;
    const uint8* bytes = static_cast<const uint8*>(data);
    if (!bytes && size > 0)
    {
        return 0;
    }

    for (size_t i = 0; i < size; ++i)
    {
        hash ^= static_cast<uint64>(bytes[i]);
        hash *= FNV_PRIME;
    }
    return hash;
}

inline uint64 ComputePakHash64(std::span<const uint8_t> bytes) noexcept
{
    return ComputePakHash64(bytes.data(), bytes.size());
}

// -----------------------------------------------------------------------------
// 대소문자 변환 유틸리티 (Windows 파일시스템 대소문자 무시 충돌 검사용)
// -----------------------------------------------------------------------------
inline std::string ToLowerPakUri(std::string_view s)
{
    std::string lower;
    lower.reserve(s.size());
    for (char ch : s)
    {
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return lower;
}

// -----------------------------------------------------------------------------
// URI 정규화 및 검증
// -----------------------------------------------------------------------------
[[nodiscard]] inline bool NormalizePakUri(std::string_view uri, std::string& outNormalized)
{
    outNormalized.clear();
    if (uri.empty() || uri.size() > PAK_MAX_URI_LENGTH)
    {
        return false;
    }

    // 1. 역슬래시를 슬래시로 변환한 작업 문자열 생성
    std::string converted;
    converted.reserve(uri.size());
    for (char ch : uri)
    {
        converted.push_back(ch == '\\' ? '/' : ch);
    }

    // 2. 허용 마운트(Project://, Engine://, Mount://) 스키마 검증
    // 대소문자 무관하게 "project://", "engine://" 또는 "mount://" 시작 확인
    constexpr std::string_view PROJECT_PREFIX = "project://";
    constexpr std::string_view ENGINE_PREFIX = "engine://";
    constexpr std::string_view MOUNT_PREFIX = "mount://";

    std::string schemePrefix;
    std::string_view remaining;
    bool isMountScheme = false;

    auto startsWithIgnoreCase = [](std::string_view text, std::string_view prefix) -> bool {
        if (text.size() < prefix.size())
        {
            return false;
        }
        for (size_t i = 0; i < prefix.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(text[i])) !=
                std::tolower(static_cast<unsigned char>(prefix[i])))
            {
                return false;
            }
        }
        return true;
    };

    if (startsWithIgnoreCase(converted, PROJECT_PREFIX))
    {
        schemePrefix = "Project://";
        remaining = std::string_view(converted).substr(PROJECT_PREFIX.size());
    }
    else if (startsWithIgnoreCase(converted, ENGINE_PREFIX))
    {
        schemePrefix = "Engine://";
        remaining = std::string_view(converted).substr(ENGINE_PREFIX.size());
    }
    else if (startsWithIgnoreCase(converted, MOUNT_PREFIX))
    {
        schemePrefix = "Mount://";
        remaining = std::string_view(converted).substr(MOUNT_PREFIX.size());
        isMountScheme = true;
    }
    else
    {
        // 허용되지 않은 스키마이거나 일반 절대/상대 경로
        return false;
    }

    // 3. 상대 경로 세그먼트 파싱 및 정규화
    // 세그먼트 내 ".." 탈출, 드라이브 문자(절대 경로 시도), 제어 문자 거부
    std::vector<std::string_view> segments;
    size_t start = 0;
    while (start < remaining.size())
    {
        // 선행 슬래시 건너뛰기
        while (start < remaining.size() && remaining[start] == '/')
        {
            ++start;
        }
        if (start >= remaining.size())
        {
            break;
        }

        size_t end = remaining.find('/', start);
        if (end == std::string_view::npos)
        {
            end = remaining.size();
        }

        std::string_view segment = remaining.substr(start, end - start);
        start = end;

        if (segment == ".")
        {
            // 현재 디렉터리 참조는 건너뜀
            continue;
        }
        if (segment == "..")
        {
            // 상위 디렉터리 탈출 시도 거부
            return false;
        }

        // 드라이브 문자/콜론(':') 포함 여부 확인 (절대 경로 탈출 방지)
        if (segment.find(':') != std::string_view::npos)
        {
            return false;
        }

        // 유효하지 않은 제어 문자(0x00 ~ 0x1F) 검사
        for (char ch : segment)
        {
            if (static_cast<unsigned char>(ch) < 0x20)
            {
                return false;
            }
        }

        segments.push_back(segment);
    }

    // 세그먼트가 하나도 없으면 빈 경로이므로 거부 (Mount://는 최소 <Alias>/<relPath> 2개 세그먼트 필요)
    if (segments.empty() || (isMountScheme && segments.size() < 2))
    {
        return false;
    }

    // 4. 최종 정규화 URI 조립
    std::string result = schemePrefix;
    for (size_t i = 0; i < segments.size(); ++i)
    {
        if (i > 0)
        {
            result.push_back('/');
        }
        result.append(segments[i]);
    }

    if (result.size() > PAK_MAX_URI_LENGTH)
    {
        return false;
    }

    outNormalized = std::move(result);
    return true;
}

END

#pragma once

#include "Engine_API.h"
#include "TextureBinaryHeader.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Engine
{
    // RGBA8 2D 텍스처의 밉 구성. 큰 밉부터 순서대로 센다.
    struct ENGINE_API TextureMipLayout
    {
        uint32 Width = 0;
        uint32 Height = 0;
        uint32 MipLevels = 0;

        bool operator==(const TextureMipLayout&) const = default;
        [[nodiscard]] bool IsValid() const;
        [[nodiscard]] uint32 GetWidth(uint32 mip) const { return Width >> mip > 1u ? Width >> mip : 1u; }
        [[nodiscard]] uint32 GetHeight(uint32 mip) const { return Height >> mip > 1u ? Height >> mip : 1u; }
        // firstMip부터 끝까지의 바이트 수
        [[nodiscard]] uint64 GetTailBytes(uint32 firstMip) const;
        // 긴 변이 size 이하인 첫 밉. 없으면 가장 작은 밉.
        [[nodiscard]] uint32 FindMipForSize(uint32 size) const;
    };

    // 일부 밉만 담은 픽셀. Header의 크기와 밉 수는 담긴 범위를 가리킨다.
    struct TextureMipData
    {
        TextureBinaryHeader Header;
        std::vector<uint8> Pixels;
    };

    // 원본을 해석한 밉 전체를 프로젝트의 .bam/cache/textures에 보관하고 필요한 밉만 읽는다.
    // 작은 밉부터 저장하므로 앞부분만 읽으면 된다. 원본의 크기나 수정 시각이 바뀌면 쓰지 않는다.
    // 지워도 다음에 원본을 읽을 때 다시 만들며, 쓸 수 없으면 원본을 매번 해석한다. 작업 스레드에서 호출해도 된다.
    namespace TextureMipCache
    {
        // 처음에 대신 보여 주는 텍스처의 최대 크기
        constexpr uint32 PlaceholderSize = 64;

        ENGINE_API std::optional<TextureMipLayout> ReadLayout(const std::wstring& key, const std::filesystem::path& source);
        ENGINE_API std::optional<TextureMipData> Load(const std::wstring& key, const std::filesystem::path& source, uint32 firstMip);
        ENGINE_API bool Store(const std::wstring& key, const std::filesystem::path& source, const TextureBinaryHeader& header, const std::vector<uint8>& pixels);
        // 밉 전체에서 firstMip부터 끝까지 잘라낸다. 형식이 다르거나 픽셀이 부족하면 비어 있다.
        ENGINE_API std::optional<TextureMipData> Slice(const TextureBinaryHeader& header, const std::vector<uint8>& pixels, uint32 firstMip);
        // 캐시가 없을 때 쓰는 단색. 재질 텍스처 슬롯에 맞는 중립 값을 고른다.
        ENGINE_API TextureMipData MakeSolid(uint32 materialSlot);
    }
}

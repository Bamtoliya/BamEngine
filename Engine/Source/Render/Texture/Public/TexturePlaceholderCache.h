#pragma once

#include "Engine_API.h"
#include "TextureBinaryHeader.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Engine
{
    // 원본을 읽는 동안 대신 쓰는 작은 RGBA8 텍스처
    struct TexturePlaceholder
    {
        TextureBinaryHeader Header;
        std::vector<uint8> Pixels;
    };

    // 원본의 작은 밉을 프로젝트의 .bam/cache/textures에 보관한다. 원본의 크기나 수정 시각이 바뀌면 쓰지 않는다.
    // 지워도 다음에 원본을 읽을 때 다시 만든다. 작업 스레드에서 호출해도 된다.
    namespace TexturePlaceholderCache
    {
        constexpr uint32 MaxSize = 64;

        ENGINE_API std::optional<TexturePlaceholder> Load(const std::wstring& key, const std::filesystem::path& source);
        ENGINE_API bool Store(const std::wstring& key, const std::filesystem::path& source, const TexturePlaceholder& placeholder);
        // MaxSize 이하인 밉부터 끝까지. 형식이 다르거나 그런 밉이 없으면 비어 있다.
        ENGINE_API std::optional<TexturePlaceholder> Extract(const TextureBinaryHeader& header, const std::vector<uint8>& pixels);
        // 캐시가 없을 때 쓰는 단색. 재질 텍스처 슬롯에 맞는 중립 값을 고른다.
        ENGINE_API TexturePlaceholder MakeSolid(uint32 materialSlot);
    }
}

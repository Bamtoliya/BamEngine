#pragma once

#include "LocalizationManager.h"
#include "ResourceManager.h"
#include <reflection/Metadata.h>
#include <cstring>
#include <optional>

namespace Editor
{
    inline std::string LocalizeInspectorText(const std::string& key)
    {
        auto text = Engine::LocalizationManager::Get().GetText(key);
        return text.empty() ? key : text;
    }

    inline std::string InspectorMetadataText(const reflection::Metadata& metadata, const char* key,
        std::string fallback = {})
    {
        const auto* value = metadata.Find(key);
        const auto* text = value != nullptr ? std::get_if<std::string>(value) : nullptr;
        return text != nullptr && !text->empty() ? *text : std::move(fallback);
    }

    // ImGui payloads are byte buffers. Check length, alignment and termination before reading a path.
    inline std::optional<std::wstring> DecodeInspectorResourcePath(const void* data, int byteCount)
    {
        if (data == nullptr || byteCount < 2 * sizeof(wchar_t) || byteCount % sizeof(wchar_t) != 0)
            return std::nullopt;
        std::wstring path(static_cast<std::size_t>(byteCount) / sizeof(wchar_t), L'\0');
        std::memcpy(path.data(), data, byteCount);
        if (path.back() != L'\0') return std::nullopt;
        path.pop_back();
        if (path.empty() || path.find(L'\0') != std::wstring::npos) return std::nullopt;
        return path;
    }

    template<typename T>
    bool TryCommitInspectorCollider(T& destination, T candidate)
    {
        if (!candidate.SetOffset(candidate.GetOffset()) || !candidate.SetSettings(candidate.GetSettings()) ||
            !candidate.SetShape(candidate.GetShape())) return false;
        destination = std::move(candidate);
        return true;
    }

    template<typename T>
    bool TryAssignInspectorResource(Engine::ResourceHandle<T>& destination, const Engine::Handle& handle)
    {
        auto& manager = Engine::ResourceManager::Get();
        if (!handle.IsValid()) return false;

        // 1. 기본 타입 매칭
        if (dynamic_cast<T*>(manager.GetResource(handle)) != nullptr)
        {
            destination = Engine::ResourceHandle<T>(handle);
            return true;
        }

        // 2. Texture -> Sprite 드롭 자동 처리
        if constexpr (std::is_same_v<T, Engine::Sprite>)
        {
            if (auto* texture = dynamic_cast<Engine::Texture*>(manager.GetResource(handle)))
            {
                std::filesystem::path texPath(texture->GetPath());
                std::filesystem::path spritePath = texPath.parent_path() / (texPath.stem().string() + ".bamsprite.json");

                // 이미 .bamsprite.json이 디스크에 있거나 로드되어 있다면 그것을 사용
                auto spriteHandle = manager.GetResourceHandle<Engine::Sprite>(spritePath.wstring());
                if (!spriteHandle)
                {
                    // 없다면 텍스처 기반으로 생성 후 json 저장
                    SpriteCreateDesc desc = {};
                    desc.texture = Engine::ResourceHandle<Engine::Texture>(handle);
                    desc.Key = spritePath.wstring();
                    desc.Path = spritePath.wstring();
                    spriteHandle = manager.LoadResource<Engine::Sprite>(&desc);
                    if (spriteHandle) manager.SaveToJsonFile(spriteHandle.Get(), spritePath.wstring());
                }

                if (spriteHandle)
                {
                    destination = spriteHandle;
                    return true;
                }
            }
        }

        return false;
    }
}

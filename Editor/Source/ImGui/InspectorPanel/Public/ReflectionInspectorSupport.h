#pragma once

#include "LocalizationManager.h"
#include "ResourceManager.h"
#include <reflection/Metadata.h>
#include <cstring>
#include <optional>
#include <reflection/PropertyInfo.h>
#include <cmath>
#include <span>
#include <type_traits>
#include <vector>
#include <reflection/EnumInfo.h>
#include <utility>
#include <variant>

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

    struct InspectorBatchWriteResult
    {
        bool Changed = false;
        bool Failed = false;
    };

    // 편집 허용 조건과 컴포넌트별 검증은 호출자가 처리합니다.
    template<typename T>
        requires (std::is_same_v<T, bool> || std::is_same_v<T, float>)
    InspectorBatchWriteResult TryWriteCommonInspectorValue(const reflection::PropertyInfo& property,
        std::span<const reflection::ObjectView> objects, T candidate)
    {
        if (objects.empty() || !property.IsValueType<T>() || !property.CanWrite())
            return { false, true };

        if constexpr (std::is_same_v<T, float>)
        {
            if (!std::isfinite(candidate)) return { false, true };
        }

        std::vector<T> previous;
        previous.reserve(objects.size());

        // 모든 대상의 접근 가능 여부를 확인한 뒤 쓰기를 시작합니다.
        for (const auto& object : objects)
        {
            if (object.IsReadOnly()) return { false, true };

            const auto read = property.TryReadValue(object);
            const auto* current = read.Value.Get<T>();
            if (!read || !current) return { false, true };

            previous.push_back(*current);
        }

        bool changed = false;

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            // vector<bool>의 프록시를 ValueView에 전달하지 않도록 실제 값으로 복사합니다.
            const T original = previous[i];
            if (original == candidate) continue;

            const auto error = property.TryWriteValue(objects[i], reflection::ValueView::From(candidate));
            if (error != reflection::PropertyAccessError::None)
            {
                bool restored = true;

                for (std::size_t j = 0; j < i; ++j)
                {
                    const T backup = previous[j];
                    if (backup == candidate) continue;

                    if (property.TryWriteValue(objects[j], reflection::ValueView::From(backup)) !=
                        reflection::PropertyAccessError::None)
                    {
                        restored = false;
                    }
                }

                return { !restored, true };
            }

            changed = true;
        }

        return { changed, false };
    }

    inline InspectorBatchWriteResult TryWriteCommonInspectorEnumValue(const reflection::PropertyInfo& property,
        const reflection::EnumInfo& enumeration, std::span<const reflection::ObjectView> objects,
        const reflection::EnumValue& candidate)
    {
        if (objects.empty() || !property.CanWrite() || !property.CanWriteEnumValue() ||
            property.GetValueCppType() != enumeration.GetCppType() || !enumeration.FindEntryByValue(candidate))
        {
            return { false, true };
        }

        const auto same = [](const reflection::EnumValue& left, const reflection::EnumValue& right)
            {
                return std::visit([](auto a, auto b) { return std::cmp_equal(a, b); }, left, right);
            };

        std::vector<reflection::EnumValue> previous;
        previous.reserve(objects.size());

        // 모든 대상의 타입과 접근 가능 여부를 먼저 확인합니다.
        for (const auto& object : objects)
        {
            if (object.IsReadOnly()) return { false, true };

            const auto read = property.TryReadValue(object);
            if (!read) return { false, true };

            const auto current = enumeration.ReadValue(read.Value);
            if (!current) return { false, true };

            previous.push_back(*current);
        }

        bool changed = false;

        for (std::size_t i = 0; i < objects.size(); ++i)
        {
            if (same(previous[i], candidate)) continue;

            if (property.TryWriteEnumValue(objects[i], candidate) != reflection::PropertyAccessError::None)
            {
                bool restored = true;

                for (std::size_t j = 0; j < i; ++j)
                {
                    if (same(previous[j], candidate)) continue;

                    if (property.TryWriteEnumValue(objects[j], previous[j]) != reflection::PropertyAccessError::None)
                        restored = false;
                }

                return { !restored, true };
            }

            changed = true;
        }

        return { changed, false };
    }
}

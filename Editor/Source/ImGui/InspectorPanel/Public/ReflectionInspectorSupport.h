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
        if (!handle.IsValid() || dynamic_cast<T*>(manager.GetResource(handle)) == nullptr) return false;
        destination = Engine::ResourceHandle<T>(handle);
        return true;
    }
}

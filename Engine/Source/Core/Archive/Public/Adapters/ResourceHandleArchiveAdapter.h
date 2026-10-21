#pragma once

#include "ResourceManager.h"
#include <archive/ArchiveInterface.h>
#include <reflection_archive/PropertyArchiveCodecs.h>
#include <array>
#include <filesystem>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

namespace archive_resource
{
    inline Engine::Handle ResolveResourceHandle(Engine::ResourceManager& manager,
        const std::wstring& assetKey, const std::wstring& assetPath)
    {
        auto handle = manager.FindHandleByKey(assetKey);
        if (manager.IsValid(handle)) return handle;
        if (!assetPath.empty())
        {
            handle = manager.FindHandleByKey(assetPath);
            if (manager.IsValid(handle)) return handle;
        }

        static const std::array<std::wstring_view, 10> candidateExtensions = {
            L".bammat.json", L".bammat",
            L".bammatinst.json", L".bammatinst",
            L".bamsprite.json", L".bamsprite",
            L".bammesh.json", L".bammesh",
            L".json", L".beve"
        };

        // 1. 이미 로드된 리소스 중 확장자가 붙은 키 탐색
        for (const auto& ext : candidateExtensions)
        {
            handle = manager.FindHandleByKey(assetKey + std::wstring(ext));
            if (manager.IsValid(handle)) return handle;
        }

        const std::filesystem::path keyPath(assetKey);
        const std::wstring keyExt = keyPath.extension().wstring();
        if (keyExt.rfind(L".bam", 0) == 0)
        {
            handle = manager.FindHandleByKey(keyPath.stem().wstring());
            if (manager.IsValid(handle)) return handle;
            std::filesystem::path stripped = keyPath;
            stripped.replace_extension();
            handle = manager.FindHandleByKey(stripped.wstring());
            if (manager.IsValid(handle)) return handle;
            handle = manager.FindHandleByKey(assetKey + L".json");
            if (manager.IsValid(handle)) return handle;
            handle = manager.FindHandleByKey(assetKey + L".beve");
            if (manager.IsValid(handle)) return handle;
        }

        // 2. assetPath가 있으면 파일 로드 시도
        if (!assetPath.empty())
        {
            const std::filesystem::path fsPath(assetPath);
            const std::wstring pathExt = fsPath.extension().wstring();
            if (pathExt.rfind(L".bam", 0) == 0)
            {
                if (std::filesystem::exists(assetPath + L".json"))
                    return manager.LoadFile(assetPath + L".json");
                if (std::filesystem::exists(assetPath + L".beve"))
                    return manager.LoadFile(assetPath + L".beve");
            }
            handle = manager.LoadFile(assetPath);
            if (manager.IsValid(handle)) return handle;
        }

        // 3. assetPath가 비어있고 assetKey만 있을 때, 디스크 파일 탐색 및 자동 로드
        for (const auto& ext : candidateExtensions)
        {
            const std::wstring candidatePath = assetKey + std::wstring(ext);
            if (std::filesystem::exists(candidatePath))
            {
                handle = manager.LoadFile(candidatePath);
                if (manager.IsValid(handle)) return handle;
            }
        }

        return handle;
    }

    // Handles are process-local; persist asset identity and resolve it through the engine resource manager.
    template<typename T>
    archive::ArchiveResult WriteHandle(archive::IArchive& ar, std::string_view key,
        const Engine::ResourceHandle<T>& value)
    {
        const bool empty = !value.GetRawHandle().IsValid();
        const Engine::Resource* resource = nullptr;
        if (!empty)
        {
            if (!Engine::ResourceManager::IsCreated())
                return { archive::ArchiveErrorCode::InvalidOperation, std::string(key) };
            resource = Engine::ResourceManager::Get().GetResource(value.GetRawHandle());
            if (!resource || !dynamic_cast<const T*>(resource) ||
                (resource->GetKey().empty() && resource->GetPath().empty()))
                return { archive::ArchiveErrorCode::InvalidValue, std::string(key) };
        }
        return ar.WriteObject(key, empty, [&](archive::IArchive& object, bool isNull) -> archive::ArchiveResult
        {
            auto result = object.Write("Null", isNull);
            if (result && !isNull) result = object.Write("AssetKey", resource->GetKey());
            if (result && !isNull) result = object.Write("AssetPath", resource->GetPath());
            return result;
        });
    }

    template<typename T>
    archive::ArchiveResult ReadHandle(const archive::IArchive& ar, std::string_view key,
        Engine::ResourceHandle<T>& value)
    {
        bool empty = true;
        Engine::ResourceHandle<T> candidate;
        const auto result = ar.ReadObject(key, empty,
            [&](archive::IArchive& object, bool& isNull) -> archive::ArchiveResult
        {
            bool hasNull = false;
            auto read = object.HasField("Null", hasNull);
            if (!read) return read;
            if (hasNull)
            {
                read = object.Read("Null", isNull);
                if (!read || isNull) return read;
            }
            std::wstring assetKey, assetPath;
            read = object.Read("AssetKey", assetKey);
            if (read) read = object.Read("AssetPath", assetPath);
            if (!read) return read;
            if (!hasNull && assetKey.empty() && assetPath.empty())
            {
                isNull = true;
                return {};
            }
            if (!Engine::ResourceManager::IsCreated())
                return { archive::ArchiveErrorCode::InvalidOperation, "ResourceManager" };
            auto& manager = Engine::ResourceManager::Get();
            const auto handle = ResolveResourceHandle(manager, assetKey, assetPath);
            if (!manager.IsValid(handle) || !dynamic_cast<T*>(manager.GetResource(handle)))
                return { archive::ArchiveErrorCode::InvalidValue, "ResourceReference" };
            candidate = Engine::ResourceHandle<T>(handle);
            return {};
        });
        if (result) value = std::move(candidate);
        return result;
    }

    template<typename T>
    bool EnsureHandleCodec(archive_reflection::PropertyCodecs& codecs)
    {
        using ResourceType = std::remove_cv_t<T>;
        if constexpr (std::is_base_of_v<Engine::Resource, ResourceType> &&
            !std::is_same_v<Engine::Resource, ResourceType>)
        {
            const std::type_index handleType(typeid(Engine::ResourceHandle<ResourceType>));
            if (codecs.HasCodec(handleType))
                return true;
            return codecs.Register<Engine::ResourceHandle<ResourceType>>(
                WriteHandle<ResourceType>, ReadHandle<ResourceType>);
        }
        return true;
    }

    template<typename T>
    struct DiscoveredHandleRegistration
    {
        static bool Register(archive_reflection::PropertyCodecs& codecs) { return EnsureHandleCodec<T>(codecs); }
    };

    template<typename T>
    struct DiscoveredHandleRegistration<Engine::ResourceHandle<T>>
    {
        static bool Register(archive_reflection::PropertyCodecs& codecs) { return EnsureHandleCodec<T>(codecs); }
    };

    template<typename Element, typename Allocator>
    struct DiscoveredHandleRegistration<std::vector<Element, Allocator>>
    {
        static bool Register(archive_reflection::PropertyCodecs& codecs)
        {
            return DiscoveredHandleRegistration<Element>::Register(codecs);
        }
    };

    template<typename Element, std::size_t N>
    struct DiscoveredHandleRegistration<std::array<Element, N>>
    {
        static bool Register(archive_reflection::PropertyCodecs& codecs)
        {
            return DiscoveredHandleRegistration<Element>::Register(codecs);
        }
    };

    template<typename Key, typename Mapped, typename Compare, typename Allocator>
    struct DiscoveredHandleRegistration<std::map<Key, Mapped, Compare, Allocator>>
    {
        static bool Register(archive_reflection::PropertyCodecs& codecs)
        {
            return DiscoveredHandleRegistration<Mapped>::Register(codecs);
        }
    };

    template<typename Key, typename Mapped, typename Hash, typename Equal, typename Allocator>
    struct DiscoveredHandleRegistration<std::unordered_map<Key, Mapped, Hash, Equal, Allocator>>
    {
        static bool Register(archive_reflection::PropertyCodecs& codecs)
        {
            return DiscoveredHandleRegistration<Mapped>::Register(codecs);
        }
    };
}


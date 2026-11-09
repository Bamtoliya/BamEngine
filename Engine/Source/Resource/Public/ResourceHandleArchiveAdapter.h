#pragma once

#include "ResourceManager.h"
#include <archive/ArchiveInterface.h>
#include <reflection_archive/PropertyArchiveCodecs.h>
#include "ResourceReference.h"
#include "Runtime.h"

namespace archive_resource
{
    inline Engine::Handle ResolveResourceHandle(Engine::ResourceManager& manager,
        const std::wstring& assetKey, const std::wstring& assetPath)
    {
        auto handle = manager.FindHandleByKey(assetKey);
        if (manager.IsValid(handle)) return handle;
        handle = manager.FindHandleByFilePath(assetKey);
        if (manager.IsValid(handle)) return handle;
        if (!assetPath.empty())
        {
            handle = manager.FindHandleByKey(assetPath);
            if (manager.IsValid(handle)) return handle;
            handle = manager.FindHandleByFilePath(assetPath);
            if (manager.IsValid(handle)) return handle;
        }

        static const std::array<std::wstring_view, 10> candidateExtensions = {
            L".bammat.json", L".bammat",
            L".bammatinst.json", L".bammatinst",
            L".bamsprite.json", L".bamsprite",
            L".bammesh.json", L".bammesh",
            L".json", L".beve"
        };

        // 1. 이미 로드된 리소스 중 확장자가 붙은 키 또는 파일 경로 탐색
        for (const auto& ext : candidateExtensions)
        {
            const auto candidate = assetKey + std::wstring(ext);
            handle = manager.FindHandleByKey(candidate);
            if (manager.IsValid(handle)) return handle;
            handle = manager.FindHandleByFilePath(candidate);
            if (manager.IsValid(handle)) return handle;
        }

        const std::filesystem::path keyPath(assetKey);
        const std::wstring keyExt = keyPath.extension().wstring();
        if (keyExt.rfind(L".bam", 0) == 0)
        {
            std::filesystem::path stripped = keyPath;
            stripped.replace_extension();
            handle = manager.FindHandleByKey(stripped.wstring());
            if (manager.IsValid(handle)) return handle;
            handle = manager.FindHandleByFilePath(stripped.wstring());
            if (manager.IsValid(handle)) return handle;
            handle = manager.FindHandleByKey(assetKey + L".json");
            if (manager.IsValid(handle)) return handle;
            handle = manager.FindHandleByFilePath(assetKey + L".json");
            if (manager.IsValid(handle)) return handle;
            handle = manager.FindHandleByKey(assetKey + L".beve");
            if (manager.IsValid(handle)) return handle;
            handle = manager.FindHandleByFilePath(assetKey + L".beve");
            if (manager.IsValid(handle)) return handle;
        }

        // 2. assetPath가 있으면 파일 로드 시도
        if (!assetPath.empty())
        {
            const auto fsPath = Engine::PlatformPath::ResolveUri(assetPath);
            const std::wstring pathExt = fsPath.extension().wstring();
            if (pathExt.rfind(L".bam", 0) == 0)
            {
                if (std::filesystem::exists(fsPath.wstring() + L".json"))
                    return manager.LoadFile(assetPath + L".json");
                if (std::filesystem::exists(fsPath.wstring() + L".beve"))
                    return manager.LoadFile(assetPath + L".beve");
            }
            if (std::filesystem::exists(fsPath))
            {
                handle = manager.LoadFile(assetPath);
                if (manager.IsValid(handle)) return handle;
            }
            for (const auto& ext : candidateExtensions)
            {
                const auto candidateWithExt = Engine::PlatformPath::ResolveUri(assetPath + std::wstring(ext));
                if (std::filesystem::exists(candidateWithExt))
                {
                    handle = manager.LoadFile(assetPath + std::wstring(ext));
                    if (manager.IsValid(handle)) return handle;
                }
            }
        }
        // 3. assetPath가 비어있고 assetKey만 있을 때, 디스크 파일 탐색 및 자동 로드
        if (!assetKey.empty())
        {
            const auto keyFsPath = Engine::PlatformPath::ResolveUri(assetKey);
            std::error_code error;
            if (std::filesystem::is_regular_file(keyFsPath, error))
            {
                handle = manager.LoadFile(assetKey);
                if (manager.IsValid(handle)) return handle;
            }
            for (const auto& ext : candidateExtensions)
            {
                const auto candidateWithExt = Engine::PlatformPath::ResolveUri(assetKey + std::wstring(ext));
                if (std::filesystem::exists(candidateWithExt))
                {
                    handle = manager.LoadFile(assetKey + std::wstring(ext));
                    if (manager.IsValid(handle)) return handle;
                }
            }
        }

        for (const auto& ext : candidateExtensions)
        {
            const auto candidatePath = Engine::PlatformPath::ResolveUri(assetKey + std::wstring(ext));
            if (std::filesystem::exists(candidatePath))
            {
                handle = manager.LoadFile(assetKey + std::wstring(ext));
                if (manager.IsValid(handle)) return handle;
            }
        }

        return handle;
    }

    inline archive::ArchiveResult WriteResourceReferences(archive::IArchive& ar, std::string_view key,
        const Engine::ResourceReferenceSet& references)
    {
        auto current = references.begin();

        return ar.WriteObjectArray(key, references.size(),
            [&](archive::IArchive& object, std::size_t) -> archive::ArchiveResult
            {
                auto result = object.Write("AssetKey", current->AssetKey);
                if (result) result = object.Write("AssetPath", current->AssetPath);
                if (result) ++current;
                return result;
            });
    }

    inline archive::ArchiveResult ReadResourceReferences(const archive::IArchive& ar, std::string_view key,
        Engine::ResourceReferenceSet& references)
    {
        bool exists = false;
        auto result = ar.HasField(key, exists);
        if (!result) return result;

        Engine::ResourceReferenceSet candidate;

        if (exists)
        {
            result = ar.ReadObjectArray(key,
                [&](archive::IArchive& object, std::size_t) -> archive::ArchiveResult
                {
                    Engine::ResourceReference reference;
                    bool hasField = false;

                    auto read = object.HasField("AssetKey", hasField);
                    if (read && hasField) read = object.Read("AssetKey", reference.AssetKey);
                    if (read) read = object.HasField("AssetPath", hasField);
                    if (read && hasField) read = object.Read("AssetPath", reference.AssetPath);
                    if (!read) return read;

                    if (reference.AssetKey.empty() && reference.AssetPath.empty())
                        return { archive::ArchiveErrorCode::InvalidValue, {} };

                    if (!candidate.insert(std::move(reference)).second)
                        return { archive::ArchiveErrorCode::InvalidValue, {} };

                    return {};
                });
        }

        if (result)
            references.swap(candidate);

        return result;
    }

    [[nodiscard]] inline Engine::ResourceReferenceSet PreloadResourceReferences(
        const Engine::ResourceReferenceSet& references)
    {
        if (references.empty() || !Engine::ResourceManager::IsCreated())
            return references;

        auto& manager = Engine::ResourceManager::Get();
        Engine::ResourceReferenceSet unresolved;

        for (const auto& reference : references)
        {
            if (reference.AssetKey.empty() && reference.AssetPath.empty())
            {
                unresolved.insert(reference);
                continue;
            }

            try
            {
                const auto handle = ResolveResourceHandle(manager, reference.AssetKey, reference.AssetPath);
                if (manager.IsValid(handle))
                    continue;
            }
            catch (const std::filesystem::filesystem_error& error)
            {
                ENGINE_LOG_WARN("Resource preload path error: {}", error.what());
            }

            unresolved.insert(reference);
        }

        return unresolved;
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
        const auto result = ar.WriteObject(key, empty, [&](archive::IArchive& object, bool isNull) -> archive::ArchiveResult
            {
                auto written = object.Write("Null", isNull);
                if (written && !isNull) written = object.Write("AssetKey", resource->GetKey());
                if (written && !isNull) written = object.Write("AssetPath", resource->GetPath());
                return written;
            });

        if (result && resource)
        {
            if (auto* references = ar.GetWriteContext<Engine::ResourceReferenceSet>())
                references->insert({ resource->GetKey(), resource->GetPath() });
        }

        return result;
    }

    template<typename T>
    archive::ArchiveResult ReadHandle(const archive::IArchive& ar, std::string_view key, Engine::ResourceHandle<T>& value)
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
                bool hasField = false;
                read = object.HasField("AssetKey", hasField);
                if (read && hasField) read = object.Read("AssetKey", assetKey);
                if (read) read = object.HasField("AssetPath", hasField);
                if (read && hasField) read = object.Read("AssetPath", assetPath);
                if (!read) return read;

                if (assetKey.empty() && !assetPath.empty()) assetKey = assetPath;
                if (assetPath.empty() && !assetKey.empty()) assetPath = assetKey;

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


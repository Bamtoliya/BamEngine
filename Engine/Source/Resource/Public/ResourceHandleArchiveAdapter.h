#pragma once

#include "ResourceManager.h"
#include <archive/ArchiveInterface.h>
#include <reflection_archive/PropertyArchiveCodecs.h>
#include "ResourceReference.h"
#include "Runtime.h"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>
#include <system_error>

namespace archive_resource
{
    namespace detail
    {
        inline constexpr std::array<std::wstring_view, 10> kCandidateResourceExtensions = {
            L".bammat.json", L".bammat",
            L".bammatinst.json", L".bammatinst",
            L".bamsprite.json", L".bamsprite",
            L".bammesh.json", L".bammesh",
            L".json", L".beve"
        };

        inline bool HasBamExtension(const std::filesystem::path& path)
        {
            return path.extension().wstring().rfind(L".bam", 0) == 0;
        }

        template<typename T>
        inline std::string ExtractResourceTypeName()
        {
            std::string name = typeid(T).name();
            if (name.rfind("class ", 0) == 0) name.erase(0, 6);
            else if (name.rfind("struct ", 0) == 0) name.erase(0, 7);
            return name;
        }

        inline Engine::Handle TryFindLoadedByKeyOrPath(Engine::ResourceManager& manager,
            const std::wstring& candidate, bool allowFilePathLookup)
        {
            auto found = manager.FindHandleByKey(candidate);
            if (!manager.IsValid(found) && allowFilePathLookup)
                found = manager.FindHandleByFilePath(candidate);
            return found;
        }

        inline Engine::Handle TryLoadWithCandidateExtensions(Engine::ResourceManager& manager,
            const std::wstring& baseUri)
        {
            for (const std::wstring_view ext : kCandidateResourceExtensions)
            {
                const std::wstring candidateUri = baseUri + std::wstring(ext);
                if (Engine::PakMountManager::Get().ExistsOrOnDisk(candidateUri))
                {
                    const auto handle = manager.LoadFile(candidateUri);
                    if (manager.IsValid(handle))
                        return handle;
                }
            }
            return {};
        }

        inline Engine::Handle TryResolveLegacyResourcesPath(Engine::ResourceManager& manager,
            const std::wstring& rawPath)
        {
            if (rawPath.empty()) return {};

            std::wstring replaced = rawPath;
            const auto replaceSegment = [&](std::wstring_view from, std::wstring_view to) -> bool
            {
                const size_t pos = replaced.find(from);
                if (pos == std::wstring::npos) return false;
                replaced.replace(pos, from.size(), to);
                return true;
            };

            bool modified = replaceSegment(L"/Resources/", L"/Assets/") || replaceSegment(L"\\Resources\\", L"\\Assets\\");
            if (!modified && replaced.rfind(L"Resources/", 0) == 0)
            {
                replaced.replace(0, 10, L"Assets/");
                modified = true;
            }
            if (!modified) return {};

            const auto candidatePath = Engine::PlatformPath::ResolveUri(replaced);
            if (!Engine::PakMountManager::Get().ExistsOrOnDisk(candidatePath)) return {};

            const auto virtualUri = Engine::PlatformPath::ToVirtualUri(candidatePath);
            const auto loaded = manager.LoadFile(virtualUri);
            return manager.IsValid(loaded) ? loaded : Engine::Handle{};
        }
    }

    inline Engine::Handle ResolveResourceHandle(Engine::ResourceManager& manager,
        const std::wstring& assetKey, const std::wstring& assetPath)
    {
        // 1. 정확한 키 또는 파일 경로로 이미 로드된 리소스 탐색
        if (auto handle = detail::TryFindLoadedByKeyOrPath(manager, assetKey, true); manager.IsValid(handle))
            return handle;
        if (!assetPath.empty())
        {
            if (auto handle = detail::TryFindLoadedByKeyOrPath(manager, assetPath, true); manager.IsValid(handle))
                return handle;
        }

        // 키가 가리키는 파일이 그대로 있으면 확장자를 바꾼 후보의 파일 경로 탐색은 건너뛴다(키 탐색은 유지).
        // 처음 로드하는 리소스마다 후보 수만큼 전체 리소스를 훑는 비용을 막는다.
        bool hasExactFile = false;
        if (Engine::PlatformPath::IsVirtualUri(assetKey))
        {
            hasExactFile = Engine::PakMountManager::Get().ExistsOrOnDisk(assetKey);
        }

        for (const std::wstring_view ext : detail::kCandidateResourceExtensions)
        {
            const std::wstring candidate = assetKey + std::wstring(ext);
            if (auto handle = detail::TryFindLoadedByKeyOrPath(manager, candidate, !hasExactFile); manager.IsValid(handle))
                return handle;
        }

        const std::filesystem::path keyPath(assetKey);
        if (detail::HasBamExtension(keyPath))
        {
            std::filesystem::path stripped = keyPath;
            stripped.replace_extension();
            for (const auto& candidate : { stripped.wstring(), assetKey + L".json", assetKey + L".beve" })
            {
                if (auto handle = detail::TryFindLoadedByKeyOrPath(manager, candidate, !hasExactFile); manager.IsValid(handle))
                    return handle;
            }
        }

        // 2. 가상 URI 키는 현재 프로젝트 기준으로 풀리므로 저장 당시의 AssetPath보다 먼저 로드한다.
        // 절대 AssetPath는 다른 체크아웃이나 옮기기 전 위치의 파일을 가리킬 수 있다.
        if (hasExactFile)
        {
            if (auto handle = manager.LoadFile(assetKey); manager.IsValid(handle))
                return handle;
        }

        // 3. assetPath / assetKey 기준 팩·디스크 파일 탐색 및 자동 로드
        if (!assetPath.empty())
        {
            const auto fsPath = Engine::PlatformPath::ResolveUri(assetPath);
            if (detail::HasBamExtension(fsPath))
            {
                if (Engine::PakMountManager::Get().ExistsOrOnDisk(assetPath + L".json"))
                    return manager.LoadFile(assetPath + L".json");
                if (Engine::PakMountManager::Get().ExistsOrOnDisk(assetPath + L".beve"))
                    return manager.LoadFile(assetPath + L".beve");
            }
            if (Engine::PakMountManager::Get().ExistsOrOnDisk(assetPath))
            {
                if (auto handle = manager.LoadFile(assetPath); manager.IsValid(handle))
                    return handle;
            }
            if (auto handle = detail::TryLoadWithCandidateExtensions(manager, assetPath); manager.IsValid(handle))
                return handle;
        }

        if (!assetKey.empty())
        {
            if (Engine::PakMountManager::Get().ExistsOrOnDisk(assetKey))
            {
                if (auto handle = manager.LoadFile(assetKey); manager.IsValid(handle))
                    return handle;
            }
            if (auto handle = detail::TryLoadWithCandidateExtensions(manager, assetKey); manager.IsValid(handle))
                return handle;
        }

        // 4. 레거시 Resources/ -> Assets/ 경로 폴백 (과거 절대/상대 경로로 저장된 .bammodel/.bammesh 복구)
        if (auto fallback = detail::TryResolveLegacyResourcesPath(manager, assetPath); manager.IsValid(fallback))
            return fallback;
        if (auto fallback = detail::TryResolveLegacyResourcesPath(manager, assetKey); manager.IsValid(fallback))
            return fallback;

        return {};
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
        Engine::ResourceReference reference;
        if (resource)
        {
            reference.AssetKey = resource->GetKey();
            reference.AssetPath = resource->GetPath();
            const std::filesystem::path path(reference.AssetPath);
            if (!Engine::PlatformPath::IsVirtualUri(reference.AssetPath) && path.is_absolute())
            {
                const auto uri = Engine::PlatformPath::ToVirtualUri(path);
                if (Engine::PlatformPath::IsEngineUri(uri) || Engine::PlatformPath::IsProjectUri(uri))
                    reference.AssetPath = uri;
            }
        }
        const auto result = ar.WriteObject(key, empty, [&](archive::IArchive& object, bool isNull) -> archive::ArchiveResult
            {
                auto written = object.Write("Null", isNull);
                if (written && !isNull) written = object.Write("AssetKey", reference.AssetKey);
                if (written && !isNull) written = object.Write("AssetPath", reference.AssetPath);
                return written;
            });

        if (result && resource)
        {
            if (auto* context = ar.GetWriteContext<Engine::ResourceReferenceArchiveContext>())
                context->References.insert(std::move(reference));
            else if (auto* references = ar.GetWriteContext<Engine::ResourceReferenceSet>())
                references->insert(std::move(reference));
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
                {
                    if (auto* recovery = Engine::MissingResourceRecoveryContext::GetActiveContext())
                    {
                        Engine::MissingResourceEntry entry;
                        entry.EntityPersistentId = recovery->CurrentEntityPersistentId;
                        entry.EntityName = recovery->CurrentEntityName;
                        entry.ComponentType = recovery->CurrentComponentType;
                        entry.ComponentDisplayName = recovery->CurrentComponentDisplayName;
                        entry.PropertyName = std::string(key);
                        entry.ExpectedResourceType = detail::ExtractResourceTypeName<T>();
                        entry.AssetKey = assetKey;
                        entry.AssetPath = assetPath;
                        recovery->Entries.push_back(std::move(entry));

                        ENGINE_LOG_WARN("Missing resource loaded as empty handle: entity={} component={} property={} type={} key=\"{}\" path=\"{}\"",
                            recovery->CurrentEntityPersistentId, recovery->CurrentComponentType, key,
                            detail::ExtractResourceTypeName<T>(), Engine::WStrToStr(assetKey), Engine::WStrToStr(assetPath));

                        candidate.Reset();
                        isNull = true;
                        return {};
                    }
                    return { archive::ArchiveErrorCode::InvalidValue, "ResourceReference" };
                }

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


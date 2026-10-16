#pragma once

#include "ResourceManager.h"
#include <archive/ArchiveInterface.h>
#include <utility>

namespace archive_resource
{
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
            auto read = object.Read("Null", isNull);
            if (!read || isNull) return read;
            std::wstring assetKey, assetPath;
            read = object.Read("AssetKey", assetKey);
            if (read) read = object.Read("AssetPath", assetPath);
            if (!read) return read;
            if (!Engine::ResourceManager::IsCreated())
                return { archive::ArchiveErrorCode::InvalidOperation, "ResourceManager" };
            auto& manager = Engine::ResourceManager::Get();
            auto handle = manager.FindHandleByKey(assetKey);
            if (!manager.IsValid(handle) && !assetPath.empty()) handle = manager.LoadFile(assetPath);
            if (!manager.IsValid(handle) || !dynamic_cast<T*>(manager.GetResource(handle)))
                return { archive::ArchiveErrorCode::InvalidValue, "ResourceReference" };
            candidate = Engine::ResourceHandle<T>(handle);
            return {};
        });
        if (result) value = std::move(candidate);
        return result;
    }
}

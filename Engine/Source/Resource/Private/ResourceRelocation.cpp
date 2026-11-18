#include "ResourceManager.h"

bool Engine::ResourceManager::RelocateFileResources(const vector<FilePathChange>& changes,
    const std::function<bool()>& commitFiles)
{
    if (changes.empty() || !commitFiles)
        return false;
    vector<FilePathChange> inverse;
    inverse.reserve(changes.size());
    for (const auto& change : changes)
    {
        if (!change.Before.is_absolute() || !change.After.is_absolute())
            return false;
        inverse.push_back({ change.After, change.Before });
    }

    struct PreparedIdentity
    {
        Resource* Instance;
        Handle Id;
        wstring Key;
        wstring Path;
    };
    // A file callback must not acquire this lock again. Resource loads/unloads wait for the full transaction.
    unique_lock lock(m_PoolMutex);
    vector<PreparedIdentity> identities;
    auto nextIndex = m_HashToHandle;
    for (size_t index = 0; index < m_Resources.size(); ++index)
    {
        const auto& slot = m_Resources[index];
        if (!slot.IsActive || !slot.Instance)
            continue;
        auto* resource = slot.Instance;
        if (!resource->CanRelocateFilePaths(changes))
        {
            ENGINE_LOG_WARN("Resource retains unsupported relocation paths: key={}, path={}",
                WStrToStr(resource->GetKey()), WStrToStr(resource->GetPath()));
            return false;
        }
        if (resource->GetPath().empty())
            continue;
        const auto before = PlatformPath::Normalize(PlatformPath::ResolveUri(resource->GetPath()));
        const auto after = PlatformPath::RemapFilePath(before, changes);
        if (before == after)
        {
            // Even an uncached source must not replace a file whose destination has a retained cache.
            if (PlatformPath::IsFilePathAffected(before, inverse))
            {
                ENGINE_LOG_WARN("Resource relocation destination is cached: {}", WStrToStr(resource->GetPath()));
                return false;
            }
            continue;
        }

        const auto nextPath = NormalizePath(PlatformPath::ToVirtualUri(after));
        const auto keyPath = PlatformPath::ResolveUri(resource->GetKey());
        // Semantic keys remain stable. Only keys that represented the backing file are renamed.
        const auto nextKey = PlatformPath::IsSamePath(keyPath, before) ? nextPath : resource->GetKey();
        const Handle id(static_cast<uint32>(index), slot.Generation);
        const auto registered = nextIndex.find(RunTimeHash(NormalizePath(resource->GetKey())));
        if (registered == nextIndex.end() || registered->second != id)
            return false;
        identities.push_back({ resource, id, nextKey, nextPath });
        nextIndex.erase(registered);
    }
    for (const auto& identity : identities)
    {
        const auto [entry, inserted] = nextIndex.emplace(RunTimeHash(identity.Key), identity.Id);
        if (!inserted && entry->second != identity.Id)
        {
            ENGINE_LOG_WARN("Resource relocation key collision: {}", WStrToStr(identity.Key));
            return false;
        }
    }
    // No allocation follows a successful file commit. Objects, handles, generations and references stay intact.
    if (!commitFiles())
        return false;
    for (auto& identity : identities)
    {
        identity.Instance->m_Key.swap(identity.Key);
        identity.Instance->m_Path.swap(identity.Path);
    }
    m_HashToHandle.swap(nextIndex);
    return true;
}

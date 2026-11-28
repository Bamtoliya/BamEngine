#include "ResourceManager.h"

#if BAM_EDITOR
bool Engine::ResourceManager::RelocateFileResources(const vector<FilePathChange>& changes,
    const std::function<bool()>& commitFiles)
{
    if (!IsOwnerThread() || changes.empty() || !commitFiles)
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
        wstring RegisteredKey;
    };
    // A file callback must not acquire this lock again. Resource loads/unloads wait for the full transaction.
    unique_lock lock(m_PoolMutex);

    if (m_IsShuttingDown)
        return false;
    vector<PreparedIdentity> identities;
    vector<Utf8FilePathUpdate> references;
    auto nextIndex = m_KeyToHandle;
    for (size_t index = 0; index < m_Resources.size(); ++index)
    {
        const auto& slot = m_Resources[index];
        if (!slot.IsActive || !slot.Instance)
            continue;
        auto* resource = slot.Instance;
        if (!resource->PrepareFilePathRelocation(changes, references))
        {
            ENGINE_LOG_WARN("Resource retains unsupported relocation paths: key={}, path={}",
                Engine::Unicode::WideToUtf8(resource->GetKey()), Engine::Unicode::WideToUtf8(resource->GetPath()));
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
                ENGINE_LOG_WARN("Resource relocation destination is cached: {}", Engine::Unicode::WideToUtf8(resource->GetPath()));
                return false;
            }
            continue;
        }

        const auto nextPath = NormalizePath(PlatformPath::ToVirtualUri(after));
        const auto keyPath = PlatformPath::ResolveUri(resource->GetKey());
        // Semantic keys remain stable. Only keys that represented the backing file are renamed.
        const auto nextKey = PlatformPath::IsSamePath(keyPath, before) ? nextPath : resource->GetKey();
        const Handle id(static_cast<uint32>(index), slot.Generation);
        const auto registered = nextIndex.find(slot.RegisteredKey);

        if (registered == nextIndex.end() ||
            registered->second != id ||
            resource->GetKey() != slot.RegisteredKey)
        {
            ENGINE_LOG_WARN(
                "Resource relocation index mismatch: "
                "resourceKey={}, registeredKey={}, path={}, "
                "indexFound={}, handleMatched={}",
                Engine::Unicode::WideToUtf8(resource->GetKey()),
                Engine::Unicode::WideToUtf8(slot.RegisteredKey),
                Engine::Unicode::WideToUtf8(resource->GetPath()),
                registered != nextIndex.end(),
                registered != nextIndex.end() && registered->second == id);

            return false;
        }
        identities.push_back({ resource, id, nextKey, nextPath, nextKey });
        nextIndex.erase(registered);
    }
    for (const auto& identity : identities)
    {
        const auto [entry, inserted] = nextIndex.emplace(identity.Key, identity.Id);
        if (!inserted && entry->second != identity.Id)
        {
            ENGINE_LOG_WARN("Resource relocation key collision: {}", Engine::Unicode::WideToUtf8(identity.Key));
            return false;
        }
    }
    // No allocation follows a successful file commit. Objects, handles, generations and references stay intact.
    if (!commitFiles())
        return false;

    for (auto& reference : references)
        reference.Apply();

    for (auto& identity : identities)
    {
        auto& slot = m_Resources[identity.Id.GetIndex()];

        slot.RegisteredKey.swap(identity.RegisteredKey);

        identity.Instance->ApplyKey(std::move(identity.Key));
        identity.Instance->m_Path.swap(identity.Path);
    }

    m_KeyToHandle.swap(nextIndex);

    return true;
}
#endif

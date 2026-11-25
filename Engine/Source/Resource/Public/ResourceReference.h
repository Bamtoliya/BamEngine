#pragma once

#include "Base.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace Engine
{
    struct ResourceReference
    {
        std::wstring AssetKey;
        std::wstring AssetPath;

        bool operator==(const ResourceReference&) const = default;

        bool operator<(const ResourceReference& other) const
        {
            return AssetKey != other.AssetKey ? AssetKey < other.AssetKey : AssetPath < other.AssetPath;
        }
    };

    using ResourceReferenceSet = std::set<ResourceReference>;

    struct ResourceReferenceArchiveContext
    {
        ResourceReferenceSet References;
        bool WritingPrefab = false;
    };

    struct MissingResourceEntry
    {
        uint64_t EntityPersistentId = 0;
        std::wstring EntityName;
        std::string ComponentType;
        std::string ComponentDisplayName;
        std::string PropertyName;
        std::string ExpectedResourceType;
        std::wstring AssetKey;
        std::wstring AssetPath;
    };

    struct MissingResourceRecoveryContext
    {
        std::vector<MissingResourceEntry> Entries;

        uint64_t CurrentEntityPersistentId = 0;
        std::wstring CurrentEntityName;
        std::string CurrentComponentType;
        std::string CurrentComponentDisplayName;

        ENGINE_API static void SetActiveContext(MissingResourceRecoveryContext* context) noexcept;
        ENGINE_API static MissingResourceRecoveryContext* GetActiveContext() noexcept;
    };

    struct MissingResourceRecoveryScope
    {
        explicit MissingResourceRecoveryScope(MissingResourceRecoveryContext* context) noexcept
            : m_Previous(MissingResourceRecoveryContext::GetActiveContext())
        {
            MissingResourceRecoveryContext::SetActiveContext(context);
        }

        ~MissingResourceRecoveryScope()
        {
            MissingResourceRecoveryContext::SetActiveContext(m_Previous);
        }

        MissingResourceRecoveryScope(const MissingResourceRecoveryScope&) = delete;
        MissingResourceRecoveryScope& operator=(const MissingResourceRecoveryScope&) = delete;

    private:
        MissingResourceRecoveryContext* m_Previous = nullptr;
    };
}

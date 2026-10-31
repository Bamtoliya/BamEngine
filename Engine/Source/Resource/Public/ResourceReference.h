#pragma once

#include <set>
#include <string>

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
}
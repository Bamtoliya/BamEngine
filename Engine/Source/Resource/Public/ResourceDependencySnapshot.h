#pragma once

#include "Handle.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <vector>

BEGIN(Engine)

struct ResourceDependencyNode
{
    Handle Id;
    std::wstring Key;

    bool TrackingSupported = false;

    std::vector<Handle> Dependencies;
};

struct ENGINE_API ResourceDependencySnapshot
{
    std::vector<ResourceDependencyNode> Nodes;

    [[nodiscard]]
    std::vector<Handle> FindDependents(const Handle& target) const
    {
        std::vector<Handle> result;

        for (const auto& node : Nodes)
        {
            const auto it = std::find(
                node.Dependencies.begin(),
                node.Dependencies.end(),
                target);

            if (it != node.Dependencies.end())
                result.push_back(node.Id);
        }

        return result;
    }

    [[nodiscard]]
    bool HasKnownCycle() const;
};

END
#include "ResourceDependencySnapshot.h"

BEGIN(Engine)

bool ResourceDependencySnapshot::HasKnownCycle() const
{
    std::unordered_map<Handle, size_t> indices;
    indices.reserve(Nodes.size());

    for (size_t i = 0; i < Nodes.size(); ++i)
        indices.emplace(Nodes[i].Id, i);

    std::vector<size_t> incoming(Nodes.size(), 0);

    for (const auto& node : Nodes)
    {
        for (const Handle& dependency : node.Dependencies)
        {
            const auto it = indices.find(dependency);

            if (it != indices.end())
                ++incoming[it->second];
        }
    }

    std::vector<size_t> ready;
    ready.reserve(Nodes.size());

    for (size_t i = 0; i < incoming.size(); ++i)
    {
        if (incoming[i] == 0)
            ready.push_back(i);
    }

    size_t processed = 0;

    while (!ready.empty())
    {
        const size_t index = ready.back();
        ready.pop_back();

        ++processed;

        for (const Handle& dependency : Nodes[index].Dependencies)
        {
            const auto it = indices.find(dependency);

            if (it == indices.end())
                continue;

            if (--incoming[it->second] == 0)
                ready.push_back(it->second);
        }
    }

    return processed != Nodes.size();
}

END
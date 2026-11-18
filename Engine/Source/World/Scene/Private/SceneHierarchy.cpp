#include "Scene.h"
#include "WorldComponents.h"
#include "TransformSystem.h"
#include "UIComponents.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
    constexpr std::size_t MaxDepth = 256;
    bool HierarchyFinite(const glm::dmat4& value)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(value[c][r])) return false;
        return true;
    }
    bool HierarchyMatches(const glm::dmat4& left, const glm::dmat4& right)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (std::abs(left[c][r] - right[c][r]) >
                    1.0e-4 * std::max({ 1.0, std::abs(left[c][r]), std::abs(right[c][r]) })) return false;
        return HierarchyFinite(left) && HierarchyFinite(right);
    }
    // Validate runtime links before passing the graph to recursive TransformSystem traversal.
    bool ValidHierarchy(Engine::Scene& scene)
    {
        const auto& world = scene.GetRegistry();
        std::unordered_map<entt::entity, std::size_t> parentReferences;
        for (const auto parent : world.view<Engine::HierarchyComponent>())
        {
            const auto& relation = world.get<Engine::HierarchyComponent>(parent);
            for (const auto child : relation.children)
            {
                const auto* childRelation = world.valid(child) ?
                    world.try_get<Engine::HierarchyComponent>(child) : nullptr;
                if (childRelation && childRelation->parent == parent)
                    ++parentReferences[child];
            }
        }
        for (const auto entity : world.view<Engine::HierarchyComponent>())
        {
            if (!scene.FindEntity(entity)) return false;
            const auto& relation = world.get<Engine::HierarchyComponent>(entity);
            if (relation.parent != entt::null)
            {
                if (!world.valid(relation.parent) || !scene.FindEntity(relation.parent) ||
                    !world.all_of<Engine::HierarchyComponent>(relation.parent)) return false;
                const bool childUI = world.any_of<Engine::CanvasComponent, Engine::RectTransformComponent>(entity);
                const bool parentUI = world.any_of<Engine::CanvasComponent, Engine::RectTransformComponent>(relation.parent);
                if (childUI || parentUI)
                {
                    if (!world.all_of<Engine::RectTransformComponent>(entity) ||
                        world.all_of<Engine::CanvasComponent>(entity) || !parentUI) return false;
                }
                else if (!world.all_of<Engine::TransformComponent, Engine::WorldTransformComponent>(entity) ||
                    !world.all_of<Engine::TransformComponent, Engine::WorldTransformComponent>(relation.parent)) return false;
                const auto found = parentReferences.find(entity);
                if (found == parentReferences.end() || found->second != 1) return false;
            }
            std::unordered_set<entt::entity> children;
            for (const auto child : relation.children)
            {
                const auto* childRelation = world.valid(child) ?
                    world.try_get<Engine::HierarchyComponent>(child) : nullptr;
                if (!childRelation || childRelation->parent != entity || !children.insert(child).second)
                    return false;
            }
        }

        // Depth 0 means visiting; positive values mean validated root-based depth.
        std::unordered_map<entt::entity, std::size_t> depths;
        std::vector<entt::entity> chain;

        for (const auto entity : world.view<Engine::HierarchyComponent>())
        {
            if (depths.contains(entity)) continue;

            chain.clear();
            auto current = entity;
            std::size_t depth = 0;

            while (current != entt::null)
            {
                if (!world.valid(current)) return false;

                const auto [found, inserted] = depths.try_emplace(current, 0);
                if (!inserted)
                {
                    if (found->second == 0) return false;
                    depth = found->second;
                    break;
                }

                if (chain.size() >= MaxDepth) return false;
                chain.push_back(current);

                const auto* link = world.try_get<Engine::HierarchyComponent>(current);
                current = link ? link->parent : entt::null;
            }

            for (auto node = chain.rbegin(); node != chain.rend(); ++node)
            {
                if (++depth > MaxDepth) return false;
                depths.at(*node) = depth;
            }
        }

        return true;
    }

    std::optional<glm::dmat4> WorldMatrix(const entt::registry& world, entt::entity entity)
    {
        glm::mat4 result;
        if (!Engine::TransformSystem::TryGetWorldMatrix(world, entity, result)) return std::nullopt;
        return glm::dmat4(result);
    }
}

Engine::EntityReparentResult Engine::Scene::ReparentEntity(Entity& child, Entity* parent)
{
    Entity* children[] = { &child };
    return ReparentEntities(children, parent);
}

Engine::EntityReparentResult Engine::Scene::ReparentEntities(std::span<Entity* const> children, Entity* parent)
{
    using Result = EntityReparentResult;
    auto& world = m_LocalRegistry;
    const auto target = parent ? parent->GetEntityHandle() : entt::null;
    const auto belongsToScene = [&](const Entity* entity)
    {
        return entity && entity->GetScene() == this && world.valid(entity->GetEntityHandle()) &&
            FindEntity(entity->GetEntityHandle()) == entity;
    };
    if (parent && !belongsToScene(parent))
        return Result::InvalidEntity;

    std::vector<entt::entity> selection;
    std::unordered_set<entt::entity> selected;
    for (const auto* child : children)
    {
        if (!belongsToScene(child))
            return Result::InvalidEntity;
        const auto handle = child->GetEntityHandle();
        if (selected.insert(handle).second)
            selection.push_back(handle);
    }
    if (selection.empty()) return Result::Unchanged;
    if (selected.contains(target)) return Result::Cycle;
    if (!ValidHierarchy(*this)) return Result::InvalidHierarchy;

    const auto getParent = [&world](entt::entity entity)
    {
        const auto* link = world.try_get<HierarchyComponent>(entity);
        return link ? link->parent : entt::null;
    };
    std::size_t parentDepth = 0;
    for (auto current = target; current != entt::null; current = getParent(current))
    {
        if (selected.contains(current)) return Result::Cycle;
        ++parentDepth;
    }

    std::vector<entt::entity> roots;
    for (const auto handle : selection)
    {
        const auto oldParent = getParent(handle);
        if (oldParent == target)
            continue;

        auto ancestor = oldParent;
        while (ancestor != entt::null && !selected.contains(ancestor))
            ancestor = getParent(ancestor);
        if (ancestor == entt::null)
            roots.push_back(handle);
    }
    if (roots.empty()) return Result::Unchanged;

    // A UI reparent needs anchor/size preservation rather than 3D TRS decomposition.
    if (parent && (world.any_of<CanvasComponent, RectTransformComponent>(target) ||
        !world.all_of<TransformComponent, WorldTransformComponent>(target)))
        return Result::InvalidTransform;
    const auto parentWorld = WorldMatrix(world, target);
    if (!parentWorld) return Result::InvalidTransform;

    struct TransformEdit
    {
        entt::entity Handle;
        TransformComponent Before;
        TransformComponent After;
    };
    std::vector<TransformEdit> transforms;
    transforms.reserve(roots.size());
    std::vector<std::pair<entt::entity, std::size_t>> pending;
    for (const auto handle : roots)
    {
        if (world.any_of<CanvasComponent, RectTransformComponent>(handle) ||
            !world.all_of<TransformComponent, WorldTransformComponent>(handle))
            return Result::InvalidTransform;

        pending.emplace_back(handle, parentDepth + 1);
        while (!pending.empty())
        {
            const auto [current, depth] = pending.back();
            pending.pop_back();
            if (depth > MaxDepth) return Result::DepthLimit;
            if (const auto* link = world.try_get<HierarchyComponent>(current))
                for (const auto descendant : link->children) pending.emplace_back(descendant, depth + 1);
        }

        const auto desiredWorld = WorldMatrix(world, handle);
        if (!desiredWorld) return Result::InvalidTransform;
        const auto before = world.get<TransformComponent>(handle);
        auto after = before;
        glm::dmat4 local;
        glm::mat4 rebuilt;
        if (!TransformSystem::TryGetLocalMatrix(glm::mat4(*parentWorld), after.absolute, glm::mat4(*desiredWorld), local) ||
            !TransformSystem::TryAssignLocalMatrix(after, local) ||
            !TransformSystem::TryComposeWorld(after, glm::mat4(*parentWorld), rebuilt) ||
            !HierarchyMatches(glm::dmat4(rebuilt), *desiredWorld))
            return Result::InvalidTransform;
        transforms.push_back({ handle, before, after });
    }

    struct LinkEdit
    {
        std::optional<HierarchyComponent> Before;
        HierarchyComponent After;
    };
    std::unordered_map<entt::entity, LinkEdit> links;
    const auto stageLink = [&](entt::entity entity) -> HierarchyComponent&
    {
        auto [entry, inserted] = links.try_emplace(entity);
        if (inserted)
        {
            if (const auto* previous = world.try_get<HierarchyComponent>(entity))
            {
                entry->second.Before = *previous;
                entry->second.After = *previous;
            }
        }
        return entry->second.After;
    };

    const std::unordered_set<entt::entity> moving(roots.begin(), roots.end());
    for (const auto handle : roots)
    {
        const auto oldParent = getParent(handle);
        if (oldParent != entt::null) stageLink(oldParent);
        stageLink(handle).parent = target;
    }
    for (auto& entry : links)
        std::erase_if(entry.second.After.children, [&moving](auto child) { return moving.contains(child); });
    if (target != entt::null)
    {
        auto& destination = stageLink(target).children;
        destination.insert(destination.end(), roots.begin(), roots.end());
    }

    // The transform solver computes the new hierarchy before publishing world matrices.
    const auto restore = [&]()
    {
        for (const auto& edit : transforms)
            world.get<TransformComponent>(edit.Handle) = edit.Before;
        for (auto& [entity, link] : links)
        {
            if (link.Before)
                world.get<HierarchyComponent>(entity) = std::move(*link.Before);
            else
                world.remove<HierarchyComponent>(entity);
        }
    };
    try
    {
        for (auto& [entity, link] : links)
            world.get_or_emplace<HierarchyComponent>(entity) = std::move(link.After);
        for (const auto& edit : transforms)
            world.get<TransformComponent>(edit.Handle) = edit.After;
        if (TransformSystem::UpdateHierarchy(world))
            return Result::Success;
    }
    catch (...)
    {
        restore();
        throw;
    }
    restore();
    return Result::InvalidTransform;
}

Engine::SiblingReorderResult Engine::Scene::ReorderSiblings(
    std::span<const entt::entity> entities, entt::entity target, bool after)
{
    using Result = SiblingReorderResult;
    if (entities.empty()) return Result::Unchanged;

    auto& world = m_LocalRegistry;
    const auto* targetLink = world.valid(target) ? world.try_get<HierarchyComponent>(target) : nullptr;
    if (!targetLink || targetLink->parent == entt::null) return Result::Invalid;
    const auto parent = targetLink->parent;
    auto* parentLink = world.valid(parent) ? world.try_get<HierarchyComponent>(parent) : nullptr;
    if (!parentLink || !FindEntity(parent)) return Result::Invalid;

    const std::unordered_set<entt::entity> selected(entities.begin(), entities.end());
    std::unordered_set<entt::entity> seen;
    std::vector<entt::entity> moving;
    std::vector<entt::entity> remaining;
    moving.reserve(selected.size());
    remaining.reserve(parentLink->children.size());
    for (const auto sibling : parentLink->children)
    {
        const auto* link = world.valid(sibling) ? world.try_get<HierarchyComponent>(sibling) : nullptr;
        if (sibling == parent || !link || link->parent != parent || !FindEntity(sibling) || !seen.insert(sibling).second)
            return Result::Invalid;
        (selected.contains(sibling) ? moving : remaining).push_back(sibling);
    }
    if (moving.size() != selected.size() || !seen.contains(target)) return Result::Invalid;
    if (selected.contains(target)) return Result::Unchanged;

    const auto insertion = std::ranges::find(remaining, target) + (after ? 1 : 0);
    remaining.insert(insertion, moving.begin(), moving.end());
    if (remaining == parentLink->children) return Result::Unchanged;
    parentLink->children.swap(remaining);
    return Result::Success;
}

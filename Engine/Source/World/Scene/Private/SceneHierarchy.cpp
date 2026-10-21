#include "Scene.h"
#include "CoreComponents.h"
#include "TransformSystem.h"
#include "UIComponents.h"
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace
{
    constexpr std::size_t MaxDepth = 256;
    bool Finite(const glm::dmat4& value)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (!std::isfinite(value[c][r])) return false;
        return true;
    }
    bool Matches(const glm::dmat4& left, const glm::dmat4& right)
    {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r)
                if (std::abs(left[c][r] - right[c][r]) >
                    1.0e-4 * std::max({ 1.0, std::abs(left[c][r]), std::abs(right[c][r]) })) return false;
        return Finite(left) && Finite(right);
    }
    // Validate runtime links before passing the graph to recursive TransformSystem traversal.
    bool ValidHierarchy(Engine::Scene& scene)
    {
        const auto& world = scene.GetRegistry();
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
                const auto& siblings = world.get<Engine::HierarchyComponent>(relation.parent).children;
                if (std::count(siblings.begin(), siblings.end(), entity) != 1) return false;
            }
            const auto& childList = relation.children;
            for (std::size_t i = 0; i < childList.size(); ++i)
            {
                const auto child = childList[i];
                const auto* childRelation = world.valid(child) ?
                    world.try_get<Engine::HierarchyComponent>(child) : nullptr;
                const bool duplicate = std::find(childList.begin(), childList.begin() + static_cast<std::ptrdiff_t>(i), child) !=
                    childList.begin() + static_cast<std::ptrdiff_t>(i);
                if (!childRelation || childRelation->parent != entity || duplicate)
                    return false;
            }
            std::array<entt::entity, MaxDepth> chain{};
            std::size_t depth = 0;
            auto current = entity;
            while (current != entt::null)
            {
                const bool cycle = std::find(chain.begin(), chain.begin() + depth, current) != chain.begin() + depth;
                if (!world.valid(current) || cycle || depth >= MaxDepth)
                    return false;
                chain[depth++] = current;
                const auto* link = world.try_get<Engine::HierarchyComponent>(current);
                current = link ? link->parent : entt::null;
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
    using Result = EntityReparentResult;
    auto& world = m_LocalRegistry;
    const auto handle = child.GetEntityHandle();
    const auto target = parent ? parent->GetEntityHandle() : entt::null;
    if (child.GetScene() != this || !world.valid(handle) || FindEntity(handle) != &child ||
        (parent && (parent->GetScene() != this || !world.valid(target) || FindEntity(target) != parent)))
        return Result::InvalidEntity;
    if (handle == target) return Result::Cycle;
    if (!ValidHierarchy(*this)) return Result::InvalidHierarchy;
    const auto* previous = world.try_get<HierarchyComponent>(handle);
    const auto oldParent = previous ? previous->parent : entt::null;
    if (oldParent == target) return Result::Unchanged;
    // UI reparenting needs anchor/size preservation, not a 3D TRS decomposition.
    if (world.any_of<CanvasComponent, RectTransformComponent>(handle) ||
        (parent && world.any_of<CanvasComponent, RectTransformComponent>(target))) return Result::InvalidTransform;
    if (!world.all_of<TransformComponent, WorldTransformComponent>(handle) ||
        (parent && !world.all_of<TransformComponent, WorldTransformComponent>(target)))
        return Result::InvalidTransform;

    std::size_t parentDepth = 0;
    for (auto current = target; current != entt::null;)
    {
        if (current == handle) return Result::Cycle;
        ++parentDepth;
        const auto* link = world.try_get<HierarchyComponent>(current);
        current = link ? link->parent : entt::null;
    }
    std::vector<std::pair<entt::entity, std::size_t>> pending{ { handle, parentDepth + 1 } };
    while (!pending.empty())
    {
        const auto [current, depth] = pending.back();
        pending.pop_back();
        if (depth > MaxDepth) return Result::DepthLimit;
        if (const auto* link = world.try_get<HierarchyComponent>(current))
            for (const auto descendant : link->children) pending.emplace_back(descendant, depth + 1);
    }

    const auto desiredWorld = WorldMatrix(world, handle);
    const auto parentWorld = WorldMatrix(world, target);
    if (!desiredWorld || !parentWorld) return Result::InvalidTransform;
    auto candidate = world.get<TransformComponent>(handle);
    glm::dmat4 local;
    glm::mat4 rebuilt;
    if (!TransformSystem::TryGetLocalMatrix(glm::mat4(*parentWorld), candidate.absolute, glm::mat4(*desiredWorld), local) ||
        !TransformSystem::TryAssignLocalMatrix(candidate, local) ||
        !TransformSystem::TryComposeWorld(candidate, glm::mat4(*parentWorld), rebuilt) ||
        !Matches(glm::dmat4(rebuilt), *desiredWorld)) return Result::InvalidTransform;

    // Stage vector edits before changing either side. Allocation exceptions are outside the transaction contract.
    HierarchyComponent childLink = previous ? *previous : HierarchyComponent{};
    auto oldLink = oldParent != entt::null ? world.get<HierarchyComponent>(oldParent) : HierarchyComponent{};
    auto newLink = target != entt::null && world.all_of<HierarchyComponent>(target) ?
        world.get<HierarchyComponent>(target) : HierarchyComponent{};
    const bool hadChildLink = previous != nullptr;
    const bool hadParentLink = target != entt::null && world.all_of<HierarchyComponent>(target);
    const auto childBefore = childLink, oldBefore = oldLink, parentBefore = newLink;
    const auto transformBefore = world.get<TransformComponent>(handle);
    std::erase(oldLink.children, handle);
    if (target != entt::null) newLink.children.push_back(handle);
    childLink.parent = target;
    static_cast<void>(world.get_or_emplace<HierarchyComponent>(handle));
    if (target != entt::null) static_cast<void>(world.get_or_emplace<HierarchyComponent>(target));
    if (oldParent != entt::null) world.get<HierarchyComponent>(oldParent) = std::move(oldLink);
    if (target != entt::null) world.get<HierarchyComponent>(target) = std::move(newLink);
    world.get<HierarchyComponent>(handle) = std::move(childLink);
    world.get<TransformComponent>(handle) = candidate;
    if (!TransformSystem::UpdateHierarchy(world))
    {
        world.get<TransformComponent>(handle) = transformBefore;
        if (hadChildLink) world.get<HierarchyComponent>(handle) = childBefore;
        else world.remove<HierarchyComponent>(handle);
        if (oldParent != entt::null) world.get<HierarchyComponent>(oldParent) = oldBefore;
        if (target != entt::null)
        {
            if (hadParentLink) world.get<HierarchyComponent>(target) = parentBefore;
            else world.remove<HierarchyComponent>(target);
        }
        return Result::InvalidTransform;
    }
    return Result::Success;
}

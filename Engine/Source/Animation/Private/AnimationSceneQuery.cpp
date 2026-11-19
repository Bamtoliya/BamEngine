#include "AnimationSceneQuery.h"
#include "HierarchyComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "Skeleton.h"
#include "AnimatorComponent.h"

const Skeleton* Engine::FindSkeletonInHierarchy(const entt::registry& world, entt::entity root)
{
    if (!world.valid(root)) return nullptr;

    if (const auto* skinned = world.try_get<SkinnedMeshRendererComponent>(root))
    {
        if (const auto* skeleton = skinned->skeletonHandle.Get()) return skeleton;
    }

    const auto* hierarchy = world.try_get<HierarchyComponent>(root);
    if (!hierarchy) return nullptr;

    for (entt::entity child : hierarchy->children)
    {
        if (const auto* skeleton = Engine::FindSkeletonInHierarchy(world, child)) return skeleton;
    }

    return nullptr;
}

entt::entity Engine::FindAnimatorEntityInAncestors(const entt::registry& world, entt::entity entity)
{
    while (entity != entt::null && world.valid(entity))
    {
        if (world.all_of<AnimatorComponent>(entity))
            return entity;

        const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
        entity = hierarchy ? hierarchy->parent : entt::null;
    }

    return entt::null;
}
entt::entity Engine::FindAnimatorEntityInHierarchy(const entt::registry& world, entt::entity root)
{
    if (root == entt::null || !world.valid(root)) return entt::null;
    if (world.all_of<AnimatorComponent>(root)) return root;

    const auto* hierarchy = world.try_get<HierarchyComponent>(root);
    if (!hierarchy) return entt::null;

    for (entt::entity child : hierarchy->children)
    {
        const entt::entity found = FindAnimatorEntityInHierarchy(world, child);
        if (found != entt::null) return found;
    }
    return entt::null;
}
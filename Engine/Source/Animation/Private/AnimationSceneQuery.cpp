#include "AnimationSceneQuery.h"
#include "ComponentEnable.h"
#include "HierarchyComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "Skeleton.h"
#include "AnimatorComponent.h"

namespace
{
    const Engine::Skeleton* FindSkeleton(const entt::registry& world, entt::entity root, bool enabledOnly)
    {
        if (!world.valid(root)) return nullptr;

        const auto* skinned = enabledOnly ? Engine::TryGetEnabledComponent<Engine::SkinnedMeshRendererComponent>(world, root)
            : world.try_get<Engine::SkinnedMeshRendererComponent>(root);
        if (skinned)
        {
            if (const auto* skeleton = skinned->skeletonHandle.Get()) return skeleton;
        }

        const auto* hierarchy = world.try_get<Engine::HierarchyComponent>(root);
        if (!hierarchy) return nullptr;

        for (entt::entity child : hierarchy->children)
        {
            if (const auto* skeleton = FindSkeleton(world, child, enabledOnly)) return skeleton;
        }

        return nullptr;
    }
}

const Skeleton* Engine::FindSkeletonInHierarchy(const entt::registry& world, entt::entity root)
{
    return FindSkeleton(world, root, false);
}

const Skeleton* Engine::FindEnabledSkeletonInHierarchy(const entt::registry& world, entt::entity root)
{
    return FindSkeleton(world, root, true);
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
    if (TryGetEnabledComponent<AnimatorComponent>(world, root)) return root;

    const auto* hierarchy = world.try_get<HierarchyComponent>(root);
    if (!hierarchy) return entt::null;

    for (entt::entity child : hierarchy->children)
    {
        const entt::entity found = FindAnimatorEntityInHierarchy(world, child);
        if (found != entt::null) return found;
    }
    return entt::null;
}
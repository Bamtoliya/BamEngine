#include "AnimationSceneQuery.h"
#include "Components/Core/HierarchyComponent.h"
#include "Components/Render/SkinnedMeshRendererComponent.h"
#include "Skeleton.h"

namespace Engine
{
    const Skeleton* FindSkeletonInHierarchy(const entt::registry& world, entt::entity root)
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
            if (const auto* skeleton = FindSkeletonInHierarchy(world, child)) return skeleton;
        }

        return nullptr;
    }
}
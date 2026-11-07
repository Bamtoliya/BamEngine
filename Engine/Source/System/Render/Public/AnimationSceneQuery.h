#pragma once

#include "Engine_API.h"
#include <entt/entity/registry.hpp>

namespace Engine
{
    class Skeleton;

    [[nodiscard]] ENGINE_API const Skeleton* FindSkeletonInHierarchy(const entt::registry& world, entt::entity root);
    [[nodiscard]] ENGINE_API entt::entity FindAnimatorEntityInAncestors(const entt::registry& world, entt::entity entity);
}
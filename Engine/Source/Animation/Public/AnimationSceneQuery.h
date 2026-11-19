#pragma once

#include "Engine_API.h"
#include <entt/entity/registry.hpp>

namespace Engine
{
    class Skeleton;

    [[nodiscard]] ENGINE_API const Skeleton* FindSkeletonInHierarchy(const entt::registry& world, entt::entity root);
    [[nodiscard]] ENGINE_API entt::entity FindAnimatorEntityInAncestors(const entt::registry& world, entt::entity entity);
    // root 자신부터 자식 계층을 깊이 우선으로 내려가며 처음 만나는 Animator 엔티티를 찾는다. 모델 배치 시 Animator는 모델 루트에 붙는다.
    [[nodiscard]] ENGINE_API entt::entity FindAnimatorEntityInHierarchy(const entt::registry& world, entt::entity root);
}
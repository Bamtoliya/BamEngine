#pragma once

#include "ReflectionEditState.h"
#include "Components/TransformComponent.h"

#include <entt/entity/registry.hpp>
#include <optional>

namespace Editor
{
    class TransformPropertyEditor
    {
    public:
        static ReflectionVectorEditSettings* GetSettings(entt::registry& world,
            entt::entity entity, TransformEditMode mode);

        // 호출 전에 TransformSystem::UpdateHierarchy()로 부모 행렬을 갱신해야 합니다.
        // 원본을 수정하지 않고 편집 후보를 반환합니다.
        [[nodiscard]]
        static std::optional<Engine::TransformComponent> TryConvertWorldMatrix(const entt::registry& world,
            entt::entity entity, const glm::mat4& worldMatrix, TransformEditMode mode);

        // 잠금을 적용한 후보를 기록하고 계층 행렬을 갱신합니다.
        // true는 편집 요청을 수락했다는 뜻이며, 잠금으로 실제 값이 유지될 수도 있습니다.
        [[nodiscard]]
        static bool ApplyWorldMatrix(entt::registry& world, entt::entity entity,
            const glm::mat4& worldMatrix, TransformEditMode mode);
    };
}
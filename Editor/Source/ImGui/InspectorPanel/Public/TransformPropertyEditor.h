#pragma once

#include "ReflectionEditState.h"
#include "Components/Core/TransformComponent.h"

#include <entt/entity/registry.hpp>
#include <optional>
#include <span>

namespace Editor
{
    enum class TransformBatchResult { Rejected, Unchanged, Changed };
    class TransformPropertyEditor
    {
    public:
        static bool IsLocked(entt::registry& world, entt::entity entity, TransformEditMode mode);
        static bool HasAnyLock(entt::registry& world, entt::entity entity);
        static bool ApplyCandidate(entt::registry& world, entt::entity entity, Engine::TransformComponent candidate);
        static ReflectionVectorEditSettings* GetSettings(entt::registry& world,
            entt::entity entity, TransformEditMode mode);

        static std::optional<ReflectionVectorEditSettings> GetCommonAxisSettings(entt::registry& world,
            std::span<const entt::entity> entities, TransformEditMode mode, int axis);
        static TransformBatchResult SetVectorAxis(entt::registry& world, std::span<const entt::entity> entities,
            TransformEditMode mode, int axis, float value);

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

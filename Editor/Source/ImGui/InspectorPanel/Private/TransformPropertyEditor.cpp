#include "TransformPropertyEditor.h"
#include "CoreComponents.h"
#include "TransformSystem.h"
#include "Reflection/CoreComponentReflection.h"

#include <reflection/Registry.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <variant>

namespace
{
    const char* GetTransformEditorPropertyName(Editor::TransformEditMode mode)
    {
        switch (mode)
        {
        case Editor::TransformEditMode::Translation: return "position";
        case Editor::TransformEditMode::Rotation: return "eulerRotation";
        case Editor::TransformEditMode::Scale: return "scale";
        default: return nullptr;
        }
    }

    float GetTransformEditorSnapStep(const reflection::Metadata& metadata)
    {
        const auto* value = metadata.Find("SnapStep");
        double step = 1.0;

        if (value != nullptr)
        {
            if (const auto* number = std::get_if<double>(value))
            {
                step = *number;
            }
            else if (const auto* number = std::get_if<std::int64_t>(value))
            {
                step = static_cast<double>(*number);
            }
            else if (const auto* number = std::get_if<std::uint64_t>(value))
            {
                step = static_cast<double>(*number);
            }
        }

        const float converted = static_cast<float>(step);
        return std::isfinite(converted) && converted > 0.0f ? converted : 1.0f;
    }

    bool IsFiniteTransformEditorMatrix(const glm::dmat4& matrix)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (!std::isfinite(matrix[column][row]))
                {
                    return false;
                }
            }
        }

        return true;
    }

    bool TransformEditorMatricesMatch(const glm::dmat4& left, const glm::dmat4& right)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                const double a = left[column][row];
                const double b = right[column][row];
                const double tolerance = 1.0e-4 * std::max({ 1.0, std::abs(a), std::abs(b) });

                if (!std::isfinite(a) || !std::isfinite(b) || std::abs(a - b) > tolerance)
                {
                    return false;
                }
            }
        }

        return true;
    }

    glm::vec3 PreserveTransformEditorLockedAxes(const glm::vec3& current, glm::vec3 candidate,
        const Editor::ReflectionVectorEditSettings* settings)
    {
        if (settings != nullptr)
        {
            for (int axis = 0; axis < 3; ++axis)
            {
                if (settings->LockedAxes[axis])
                {
                    candidate[axis] = current[axis];
                }
            }
        }

        return candidate;
    }
}

namespace Editor
{
    ReflectionVectorEditSettings* TransformPropertyEditor::GetSettings(entt::registry& world,
        entt::entity entity, TransformEditMode mode)
    {
        if (!world.valid(entity) || !world.all_of<Engine::TransformComponent>(entity))
        {
            return nullptr;
        }

        const char* propertyName = GetTransformEditorPropertyName(mode);
        const auto* registry = Engine::GetCoreComponentReflectionRegistry();
        const auto* type = registry != nullptr ? registry->FindType<Engine::TransformComponent>() : nullptr;
        const auto* property = type != nullptr && propertyName != nullptr ?
            type->FindProperty(propertyName) : nullptr;

        if (property == nullptr)
        {
            return nullptr;
        }

        auto& state = world.get_or_emplace<ReflectionEntityEditState>(entity);
        auto& settings = state.Components[type->QualifiedName].Vectors[propertyName];

        if (!std::isfinite(settings.SnapStep) || settings.SnapStep <= 0.0f)
        {
            settings.SnapStep = GetTransformEditorSnapStep(property->Metadata);
        }

        return &settings;
    }

    std::optional<Engine::TransformComponent> TransformPropertyEditor::TryConvertWorldMatrix(
        const entt::registry& world, entt::entity entity, const glm::mat4& worldMatrix, TransformEditMode mode)
    {
        if (!world.valid(entity))
        {
            return std::nullopt;
        }

        const auto* source = world.try_get<Engine::TransformComponent>(entity);
        const glm::dmat4 desiredWorld(worldMatrix);

        if (source == nullptr || !IsFiniteTransformEditorMatrix(desiredWorld))
        {
            return std::nullopt;
        }

        glm::dmat4 parentWorld(1.0);
        const auto* hierarchy = world.try_get<Engine::HierarchyComponent>(entity);

        if (hierarchy != nullptr && hierarchy->parent != entt::null)
        {
            const entt::entity parent = hierarchy->parent;

            if (parent == entity || !world.valid(parent))
            {
                return std::nullopt;
            }

            const auto* parentTransform = world.try_get<Engine::WorldTransformComponent>(parent);

            if (parentTransform == nullptr)
            {
                return std::nullopt;
            }

            parentWorld = glm::dmat4(parentTransform->worldMatrix);
        }

        const double determinant = glm::determinant(parentWorld);

        if (!IsFiniteTransformEditorMatrix(parentWorld) ||
            !std::isfinite(determinant) || determinant == 0.0)
        {
            return std::nullopt;
        }

        const glm::dmat4 localMatrix = glm::inverse(parentWorld) * desiredWorld;

        if (!IsFiniteTransformEditorMatrix(localMatrix))
        {
            return std::nullopt;
        }

        Engine::TransformComponent candidate = *source;

        if (!candidate.SynchronizeRotation() || !candidate.SetLocalPosition(glm::vec3(localMatrix[3])))
        {
            return std::nullopt;
        }

        switch (mode)
        {
        case TransformEditMode::Translation:
            break;

        case TransformEditMode::Rotation:
        {
            glm::dmat3 rotationMatrix(1.0);

            for (int axis = 0; axis < 3; ++axis)
            {
                const double signedScale = static_cast<double>(candidate.scale[axis]);

                if (!std::isfinite(signedScale) || signedScale == 0.0)
                {
                    return std::nullopt;
                }

                rotationMatrix[axis] = glm::dvec3(localMatrix[axis]) / signedScale;
            }

            glm::dquat desiredRotation = glm::quat_cast(rotationMatrix);
            const double length = glm::length(desiredRotation);

            if (!std::isfinite(length) || length < 1.0e-12)
            {
                return std::nullopt;
            }

            desiredRotation /= length;

            const glm::quat converted = glm::quat::wxyz(static_cast<float>(desiredRotation.w),
                static_cast<float>(desiredRotation.x), static_cast<float>(desiredRotation.y),
                static_cast<float>(desiredRotation.z));

            if (!candidate.SetLocalRotation(converted))
            {
                return std::nullopt;
            }

            break;
        }

        case TransformEditMode::Scale:
        {
            const glm::dquat rotation = glm::dquat::wxyz(candidate.rotation.w,
                candidate.rotation.x, candidate.rotation.y, candidate.rotation.z);

            const glm::dmat3 basis = glm::mat3_cast(rotation);
            glm::vec3 convertedScale(1.0f);

            for (int axis = 0; axis < 3; ++axis)
            {
                convertedScale[axis] = static_cast<float>(
                    glm::dot(basis[axis], glm::dvec3(localMatrix[axis])));
            }

            if (!candidate.SetLocalScale(convertedScale))
            {
                return std::nullopt;
            }

            break;
        }

        default:
            return std::nullopt;
        }

        const glm::dmat4 rebuiltLocal(candidate.GetLocalMatrix());

        if (!TransformEditorMatricesMatch(rebuiltLocal, localMatrix) ||
            !TransformEditorMatricesMatch(parentWorld * rebuiltLocal, desiredWorld))
        {
            return std::nullopt;
        }

        return candidate;
    }

    bool TransformPropertyEditor::ApplyWorldMatrix(entt::registry& world, entt::entity entity,
        const glm::mat4& worldMatrix, TransformEditMode mode)
    {
        auto converted = TryConvertWorldMatrix(world, entity, worldMatrix, mode);

        if (!converted)
        {
            return false;
        }

        auto& current = world.get<Engine::TransformComponent>(entity);
        auto& candidate = *converted;

        const auto* positionSettings = GetSettings(world, entity, TransformEditMode::Translation);
        const auto* rotationSettings = GetSettings(world, entity, TransformEditMode::Rotation);
        const auto* scaleSettings = GetSettings(world, entity, TransformEditMode::Scale);

        candidate.position = PreserveTransformEditorLockedAxes(current.position, candidate.position, positionSettings);
        candidate.scale = PreserveTransformEditorLockedAxes(current.scale, candidate.scale, scaleSettings);

        if (mode == TransformEditMode::Rotation && rotationSettings != nullptr)
        {
            const auto& locks = rotationSettings->LockedAxes;

            if (locks[0] && locks[1] && locks[2])
            {
                // 회전 전체가 잠겼으면 Quaternion과 Hint를 그대로 보존합니다.
                const glm::vec3 position = candidate.position;
                const glm::vec3 scale = candidate.scale;

                candidate = current;
                candidate.position = position;
                candidate.scale = scale;
            }
            else
            {
                const glm::vec3 angles = PreserveTransformEditorLockedAxes(
                    current.eulerRotation, candidate.eulerRotation, rotationSettings);

                if (angles != candidate.eulerRotation && !candidate.SetEulerRotation(angles))
                {
                    return false;
                }
            }
        }

        if (candidate.position == current.position && candidate.scale == current.scale &&
            candidate.rotation == current.rotation && candidate.eulerRotation == current.eulerRotation)
        {
            return true;
        }

        current = candidate;
        Engine::TransformSystem::UpdateHierarchy(world);
        return true;
    }
}
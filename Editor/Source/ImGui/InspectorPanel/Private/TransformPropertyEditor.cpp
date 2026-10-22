#include "TransformPropertyEditor.h"
#include "CoreComponents.h"
#include "TransformSystem.h"
#include "Reflection/ReflectionRegistry.h"

#include <reflection/Registry.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <variant>
#include <limits>
#include <vector>

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
            if (settings->Locked) return current;
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

    using TransformVectorMember = glm::vec3 Engine::TransformComponent::*;

    TransformVectorMember GetTransformEditorVectorMember(Editor::TransformEditMode mode)
    {
        switch (mode)
        {
        case Editor::TransformEditMode::Translation: return &Engine::TransformComponent::position;
        case Editor::TransformEditMode::Scale: return &Engine::TransformComponent::scale;
        case Editor::TransformEditMode::Rotation: return &Engine::TransformComponent::eulerRotation;
        default: return nullptr;
        }
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
        const auto* registry = Engine::ReflectionRegistry::IsCreated() ?
            &Engine::ReflectionRegistry::Get().GetRegistry() : nullptr;
        const auto* type = registry != nullptr ? registry->FindType<Engine::TransformComponent>() : nullptr;
        const auto* property = type != nullptr && propertyName != nullptr ?
            type->FindProperty(propertyName) : nullptr;

        if (property == nullptr)
        {
            return nullptr;
        }

        auto& state = GetReflectionEntityEditState(world, entity);
        auto& settings = state.Components[type->QualifiedName].Vectors[propertyName];

        if (!std::isfinite(settings.SnapStep) || settings.SnapStep <= 0.0f)
        {
            settings.SnapStep = GetTransformEditorSnapStep(property->Metadata);
        }

        return &settings;
    }

    std::optional<ReflectionVectorEditSettings> TransformPropertyEditor::GetCommonAxisSettings(
        entt::registry& world, std::span<const entt::entity> entities, TransformEditMode mode, int axis)
    {
        const auto member = GetTransformEditorVectorMember(mode);
        if (!member || entities.empty() || axis < 0 || axis >= 3 ||
            !Engine::ReflectionRegistry::IsCreated()) return {};

        const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
        const auto* type = registry.FindType<Engine::TransformComponent>();
        const auto* property = type ? type->FindProperty(GetTransformEditorPropertyName(mode)) : nullptr;
        if (!property || !property->CanWrite()) return {};

        const auto metadataBool = [&](const char* key, bool fallback)
            {
                const auto* metadata = property->Metadata.Find(key);
                const auto* value = metadata ? std::get_if<bool>(metadata) : nullptr;
                return value ? *value : fallback;
            };

        if (!metadataBool("Editable", false) || metadataBool("ReadOnly", false) ||
            !metadataBool("Visible", true)) return {};

        Engine::ETransformAbsolute channel = Engine::ETransformAbsolute::None;
        switch (mode)
        {
        case TransformEditMode::Translation: channel = Engine::ETransformAbsolute::Position; break;
        case TransformEditMode::Rotation: channel = Engine::ETransformAbsolute::Rotation; break;
        case TransformEditMode::Scale: channel = Engine::ETransformAbsolute::Scale; break;
        default: return {};
        }

        ReflectionVectorEditSettings common;
        unsigned firstSpace = 0;
        bool initialized = false;

        for (const auto entity : entities)
        {
            if (!world.valid(entity)) return {};
            const auto* transform = world.try_get<Engine::TransformComponent>(entity);
            if (!transform || !transform->HasValidAbsoluteFlags()) return {};
            if (mode == TransformEditMode::Rotation && !transform->IsRotationSynchronized()) return {};

            const auto* settings = GetSettings(world, entity, mode);
            if (!settings || settings->Locked || settings->LockedAxes[axis]) return {};

            const auto& component = GetReflectionEntityEditState(world, entity).Components[type->QualifiedName];
            if (component.ReadOnlyProperties.contains(property->Name)) return {};

            const unsigned space = static_cast<unsigned>(transform->absolute) & static_cast<unsigned>(channel);

            if (!initialized)
            {
                common = *settings;
                firstSpace = space;
                initialized = true;
            }
            else if (space != firstSpace || settings->SnapEnabled != common.SnapEnabled ||
                (common.SnapEnabled && settings->SnapStep != common.SnapStep))
            {
                return {};
            }
        }

        return common;
    }

    TransformBatchResult TransformPropertyEditor::SetVectorAxis(entt::registry& world,
        std::span<const entt::entity> entities, TransformEditMode mode, int axis, float value)
    {
        using Transform = Engine::TransformComponent;
        const auto member = GetTransformEditorVectorMember(mode);
        const auto settings = GetCommonAxisSettings(world, entities, mode, axis);
        if (!member || !settings || !std::isfinite(value)) return TransformBatchResult::Rejected;

        double adjusted = value;
        if (settings->SnapEnabled)
        {
            const double step = settings->SnapStep;
            if (!std::isfinite(step) || step <= 0.0) return TransformBatchResult::Rejected;
            adjusted = std::round(adjusted / step) * step;
        }

        if (!std::isfinite(adjusted) ||
            std::abs(adjusted) > (std::numeric_limits<float>::max)()) return TransformBatchResult::Rejected;

        value = static_cast<float>(adjusted);
        std::vector<Transform> previous;
        std::vector<Transform> candidates;
        previous.reserve(entities.size());
        candidates.reserve(entities.size());
        bool changed = false;

        for (const auto entity : entities)
        {
            const auto& current = world.get<Transform>(entity);
            auto candidate = current;

            if ((current.*member)[axis] != value)
            {
                auto vector = current.*member;
                vector[axis] = value;
                bool valid = false;

                switch (mode)
                {
                case TransformEditMode::Translation: valid = candidate.SetLocalPosition(vector); break;
                case TransformEditMode::Rotation: valid = candidate.SetEulerRotation(vector); break;
                case TransformEditMode::Scale: valid = candidate.SetLocalScale(vector); break;
                default: return TransformBatchResult::Rejected;
                }

                if (!valid) return TransformBatchResult::Rejected;
                changed = true;
            }

            previous.push_back(current);
            candidates.push_back(candidate);
        }

        if (!changed) return TransformBatchResult::Unchanged;

        for (std::size_t i = 0; i < entities.size(); ++i)
            world.get<Transform>(entities[i]) = candidates[i];

        if (Engine::TransformSystem::UpdateHierarchy(world)) return TransformBatchResult::Changed;

        for (std::size_t i = 0; i < entities.size(); ++i)
            world.get<Transform>(entities[i]) = previous[i];

        return TransformBatchResult::Rejected;
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

            glm::mat4 calculated;
            if (!Engine::TransformSystem::TryGetWorldMatrix(world, parent, calculated)) return std::nullopt;
            parentWorld = glm::dmat4(calculated);
        }
        glm::dmat4 localMatrix;
        if (!Engine::TransformSystem::TryGetLocalMatrix(glm::mat4(parentWorld), source->absolute,
            worldMatrix, localMatrix)) return std::nullopt;

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

        glm::mat4 rebuiltWorld;
        if (!TransformEditorMatricesMatch(rebuiltLocal, localMatrix) ||
            !Engine::TransformSystem::TryComposeWorld(candidate, glm::mat4(parentWorld), rebuiltWorld) ||
            !TransformEditorMatricesMatch(glm::dmat4(rebuiltWorld), desiredWorld))
        {
            return std::nullopt;
        }

        return candidate;
    }

    bool TransformPropertyEditor::ApplyWorldMatrix(entt::registry& world, entt::entity entity,
        const glm::mat4& worldMatrix, TransformEditMode mode)
    {
        auto converted = TryConvertWorldMatrix(world, entity, worldMatrix, mode);
        return converted && ApplyCandidate(world, entity, *converted);
    }

    bool TransformPropertyEditor::IsLocked(entt::registry& world, entt::entity entity, TransformEditMode mode)
    {
        const auto* settings = GetSettings(world, entity, mode);
        return settings && settings->Locked;
    }

    bool TransformPropertyEditor::HasAnyLock(entt::registry& world, entt::entity entity)
    {
        return IsLocked(world, entity, TransformEditMode::Translation) ||
            IsLocked(world, entity, TransformEditMode::Rotation) || IsLocked(world, entity, TransformEditMode::Scale);
    }

    bool TransformPropertyEditor::ApplyCandidate(entt::registry& world, entt::entity entity,
        Engine::TransformComponent candidate)
    {
        const auto* source = world.valid(entity) ? world.try_get<Engine::TransformComponent>(entity) : nullptr;
        if (!source || !candidate.HasValidAbsoluteFlags()) return false;
        const auto current = *source;
        if (candidate.absolute != current.absolute)
        {
            if (HasAnyLock(world, entity)) return false;
            return Engine::TransformSystem::TryCommitTransform(world, entity, candidate);
        }
        const auto* positionSettings = GetSettings(world, entity, TransformEditMode::Translation);
        const auto* rotationSettings = GetSettings(world, entity, TransformEditMode::Rotation);
        const auto* scaleSettings = GetSettings(world, entity, TransformEditMode::Scale);

        candidate.position = PreserveTransformEditorLockedAxes(current.position, candidate.position, positionSettings);
        candidate.scale = PreserveTransformEditorLockedAxes(current.scale, candidate.scale, scaleSettings);

        if (rotationSettings != nullptr)
        {
            const auto& locks = rotationSettings->LockedAxes;

            if (rotationSettings->Locked || (locks[0] && locks[1] && locks[2]))
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

        return Engine::TransformSystem::TryCommitTransform(world, entity, candidate);
    }
}

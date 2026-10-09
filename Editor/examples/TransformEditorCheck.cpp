#include "TransformPropertyEditor.h"
#include "CoreComponents.h"
#include "TransformSystem.h"

#include <iostream>
#include <limits>

namespace
{
    int FailTransformEditorCheck(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    int CheckTransformEditorAngleBoundary()
    {
        entt::registry world;
        const entt::entity entity = world.create();

        world.emplace<Engine::TransformComponent>(entity);
        world.emplace<Engine::WorldTransformComponent>(entity);

        auto& transform = world.get<Engine::TransformComponent>(entity);
        const char* axisNames[] = { "X", "Y", "Z" };

        for (int axis = 0; axis < 3; ++axis)
        {
            for (float direction : { 1.0f, -1.0f })
            {
                glm::vec3 before(0.0f);
                glm::vec3 target(0.0f);

                before[axis] = direction * 179.0f;
                target[axis] = direction * -179.0f;

                if (!transform.SetEulerRotation(before))
                {
                    return FailTransformEditorCheck("Angle boundary setup failed.");
                }

                Engine::TransformSystem::UpdateHierarchy(world);

                Engine::TransformComponent desired = transform;

                if (!desired.SetEulerRotation(target))
                {
                    return FailTransformEditorCheck("Angle boundary target setup failed.");
                }

                const bool accepted = Editor::TransformPropertyEditor::ApplyWorldMatrix(world, entity,
                    desired.GetLocalMatrix(), Editor::TransformEditMode::Rotation);

                const float expected = direction * 181.0f;

                if (!accepted || std::abs(transform.eulerRotation[axis] - expected) > 0.05f)
                {
                    std::cerr << "Angle boundary failed: axis " << axisNames[axis]
                        << ", before " << before[axis] << ", applied " << transform.eulerRotation[axis]
                        << ", expected " << expected << '\n';

                    return 1;
                }
            }
        }

        std::cout << "Transform editor angle boundary checks passed.\n";
        return 0;
    }
}

int main()
{
    if (CheckTransformEditorAngleBoundary() != 0)
    {
        return 1;
    }

    entt::registry world;
    const entt::entity parent = world.create();
    const entt::entity child = world.create();

    world.emplace<Engine::TransformComponent>(parent);
    world.emplace<Engine::WorldTransformComponent>(parent);
    world.emplace<Engine::HierarchyComponent>(parent);

    world.emplace<Engine::TransformComponent>(child);
    world.emplace<Engine::WorldTransformComponent>(child);
    world.emplace<Engine::HierarchyComponent>(child);

    world.get<Engine::HierarchyComponent>(parent).children.push_back(child);
    world.get<Engine::HierarchyComponent>(child).parent = parent;

    auto& parentTransform = world.get<Engine::TransformComponent>(parent);
    auto& childTransform = world.get<Engine::TransformComponent>(child);

    parentTransform.position = glm::vec3(3.0f, 2.0f, -1.0f);
    parentTransform.scale = glm::vec3(2.0f);
    childTransform.scale = glm::vec3(-1.0f, 2.0f, 3.0f);

    if (!parentTransform.SetEulerRotation(glm::vec3(15.0f, 35.0f, 10.0f)) ||
        !childTransform.SetEulerRotation(glm::vec3(0.0f, 350.0f, 0.0f)))
    {
        return FailTransformEditorCheck("Transform editor setup failed.");
    }

    Engine::TransformSystem::UpdateHierarchy(world);

    for (int step = 1; step <= 72; ++step)
    {
        Engine::TransformComponent desired = childTransform;
        const float degrees = 350.0f + static_cast<float>(step) * 5.0f;

        if (!desired.SetEulerRotation(glm::vec3(0.0f, degrees, 0.0f)))
        {
            return FailTransformEditorCheck("Transform editor rotation setup failed.");
        }

        const glm::vec3 previousEuler = childTransform.eulerRotation;
        const glm::mat4 desiredWorld = world.get<Engine::WorldTransformComponent>(parent).worldMatrix *
            desired.GetLocalMatrix();

        auto candidate = Editor::TransformPropertyEditor::TryConvertWorldMatrix(world, child, desiredWorld,
            Editor::TransformEditMode::Rotation);

        if (!candidate || candidate->scale != childTransform.scale ||
            childTransform.eulerRotation != previousEuler ||
            glm::length(candidate->eulerRotation - glm::vec3(0.0f, degrees, 0.0f)) > 0.05f)
        {
            return FailTransformEditorCheck("Transform editor rotation conversion failed.");
        }

        childTransform = *candidate;
        Engine::TransformSystem::UpdateHierarchy(world);
    }

    auto* positionSettings = Editor::TransformPropertyEditor::GetSettings(world, child,
        Editor::TransformEditMode::Translation);

    auto* rotationSettings = Editor::TransformPropertyEditor::GetSettings(world, child,
        Editor::TransformEditMode::Rotation);

    auto* scaleSettings = Editor::TransformPropertyEditor::GetSettings(world, child,
        Editor::TransformEditMode::Scale);

    if (positionSettings == nullptr || rotationSettings == nullptr || scaleSettings == nullptr)
    {
        return FailTransformEditorCheck("Transform editor settings lookup failed.");
    }

    positionSettings->LockedAxes[0] = true;
    rotationSettings->LockedAxes[1] = true;
    scaleSettings->LockedAxes[2] = true;

    Engine::TransformComponent desired = childTransform;
    desired.position = glm::vec3(2.0f, 3.0f, 4.0f);

    glm::mat4 desiredWorld = world.get<Engine::WorldTransformComponent>(parent).worldMatrix *
        desired.GetLocalMatrix();

    if (!Editor::TransformPropertyEditor::ApplyWorldMatrix(world, child, desiredWorld,
        Editor::TransformEditMode::Translation) ||
        glm::length(childTransform.position - glm::vec3(0.0f, 3.0f, 4.0f)) > 1.0e-4f)
    {
        return FailTransformEditorCheck("Transform editor position lock failed.");
    }

    desired = childTransform;

    if (!desired.SetEulerRotation(glm::vec3(0.0f, 740.0f, 0.0f)))
    {
        return FailTransformEditorCheck("Transform editor locked rotation setup failed.");
    }

    desiredWorld = world.get<Engine::WorldTransformComponent>(parent).worldMatrix * desired.GetLocalMatrix();

    if (!Editor::TransformPropertyEditor::ApplyWorldMatrix(world, child, desiredWorld,
        Editor::TransformEditMode::Rotation) ||
        std::abs(childTransform.eulerRotation.y - 710.0f) > 0.05f)
    {
        return FailTransformEditorCheck("Transform editor rotation lock failed.");
    }

    desired = childTransform;
    desired.scale = glm::vec3(-2.0f, 3.0f, 4.0f);

    desiredWorld = world.get<Engine::WorldTransformComponent>(parent).worldMatrix * desired.GetLocalMatrix();

    if (!Editor::TransformPropertyEditor::ApplyWorldMatrix(world, child, desiredWorld,
        Editor::TransformEditMode::Scale) ||
        glm::length(childTransform.scale - glm::vec3(-2.0f, 3.0f, 3.0f)) > 1.0e-4f)
    {
        return FailTransformEditorCheck("Transform editor scale lock failed.");
    }

    const Engine::TransformComponent saved = childTransform;
    glm::mat4 invalidLocal = childTransform.GetLocalMatrix();
    invalidLocal[1] += invalidLocal[0] * 0.25f;

    const glm::mat4 invalidWorld = world.get<Engine::WorldTransformComponent>(parent).worldMatrix * invalidLocal;

    if (Editor::TransformPropertyEditor::ApplyWorldMatrix(world, child, invalidWorld,
        Editor::TransformEditMode::Scale))
    {
        return FailTransformEditorCheck("Transform editor shear rejection failed.");
    }

    glm::mat4 nonFiniteWorld(1.0f);
    nonFiniteWorld[0][0] = std::numeric_limits<float>::quiet_NaN();

    if (Editor::TransformPropertyEditor::ApplyWorldMatrix(world, child, nonFiniteWorld,
        Editor::TransformEditMode::Translation))
    {
        return FailTransformEditorCheck("Transform editor non-finite rejection failed.");
    }

    parentTransform.scale.x = 0.0f;
    Engine::TransformSystem::UpdateHierarchy(world);

    if (Editor::TransformPropertyEditor::ApplyWorldMatrix(world, child,
        world.get<Engine::WorldTransformComponent>(child).worldMatrix, Editor::TransformEditMode::Rotation))
    {
        return FailTransformEditorCheck("Transform editor singular parent rejection failed.");
    }

    if (childTransform.position != saved.position || childTransform.scale != saved.scale ||
        childTransform.rotation != saved.rotation || childTransform.eulerRotation != saved.eulerRotation)
    {
        return FailTransformEditorCheck("Transform editor rejection changed source.");
    }

    std::cout << "Transform editor conversion checks passed.\n";
    std::cout << "Transform editor lock checks passed.\n";
    std::cout << "Transform editor rejection checks passed.\n";
    return 0;
}
#include "ThirdPersonControllerSystem.h"
#include "ComponentEnable.h"
#include "Scene.h"
#include "WorldComponents.h"
#include "CharacterControllerComponent.h"
#include "ThirdPersonControllerComponent.h"
#include "SpringArmComponent.h"
#include "InputManager.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

EResult ThirdPersonControllerSystem::Initialize(void* arg)
{
    (void)arg;
    return EResult::Success;
}

ThirdPersonControllerSystem* ThirdPersonControllerSystem::Create(void* arg)
{
    auto* system = new ThirdPersonControllerSystem();
    if (system->Initialize(arg) != EResult::Success)
    {
        Safe_Release(system);
        return nullptr;
    }
    return system;
}

void ThirdPersonControllerSystem::Free() {}

void ThirdPersonControllerSystem::OnUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt)
{
    (void)globalRegistry;
    if (!std::isfinite(dt) || dt <= 0.0f) return;

    for (Scene* scene : activeScenes)
    {
        if (scene && scene->IsActive() && !scene->IsPaused())
            UpdateControllers(*scene, dt);
    }
}

namespace
{
    struct ThirdPersonMoveInput
    {
        vec2 Axis;
        bool Sprint;
        bool JumpPressed;
    };

    ThirdPersonMoveInput ReadMoveInput(const InputManager& input)
    {
        const f32 x = static_cast<f32>(input.IsActionHeld("Gameplay", "MoveRight")) -
            static_cast<f32>(input.IsActionHeld("Gameplay", "MoveLeft"));
        const f32 z = static_cast<f32>(input.IsActionHeld("Gameplay", "MoveForward")) -
            static_cast<f32>(input.IsActionHeld("Gameplay", "MoveBackward"));

        return { vec2(x, z), input.IsActionHeld("Gameplay", "Sprint"), input.WasActionPressed("Gameplay", "Jump") };
    }

    SpringArmComponent* FindSpringArm(entt::registry& world, entt::entity entity, entt::entity configuredArm)
    {
        if (configuredArm != entt::null && world.valid(configuredArm))
        {
            if (auto* arm = TryGetEnabledComponent<SpringArmComponent>(world, configuredArm)) return arm;
        }

        if (auto* arm = TryGetEnabledComponent<SpringArmComponent>(world, entity)) return arm;

        if (const auto* hierarchy = world.try_get<HierarchyComponent>(entity))
        {
            for (const auto child : hierarchy->children)
            {
                if (!world.valid(child)) continue;
                if (auto* arm = TryGetEnabledComponent<SpringArmComponent>(world, child)) return arm;
            }
        }

        return nullptr;
    }

    void UpdateOrbit(SpringArmComponent& arm, const ThirdPersonControllerComponent& ctrl, const InputManager& input)
    {
        if (!ctrl.RequireRightClickToRotate || input.IsActionHeld("Gameplay", "OrbitCamera"))
        {
            const vec2 mouseDelta = input.GetActionMapMouseDelta("Gameplay");
            if (glm::length(mouseDelta) > 0.0001f)
            {
                arm.Yaw += mouseDelta.x * ctrl.MouseSensitivity;
                const f32 pitchSign = ctrl.InvertPitch ? 1.0f : -1.0f;
                arm.Pitch += mouseDelta.y * ctrl.MouseSensitivity * pitchSign;
                arm.Pitch = std::clamp(arm.Pitch, arm.MinPitch, arm.MaxPitch);
            }
        }

        const f32 scrollY = input.GetActionMapMouseScrollDeltaY("Gameplay");
        if (std::abs(scrollY) > 0.001f)
            arm.TargetArmLength = std::clamp(arm.TargetArmLength - scrollY * ctrl.ZoomSensitivity, 0.5f, 50.0f);
    }

    void ApplyMovement(CharacterControllerComponent& kcc, TransformComponent& transform,
        const ThirdPersonControllerComponent& ctrl, const ThirdPersonMoveInput& moveInput, f32 armYaw, f32 dt)
    {
        const quat yawRotation = quat(vec3(0.0f, glm::radians(armYaw), 0.0f));
        vec3 moveDirection = yawRotation * vec3(moveInput.Axis.x, 0.0f, moveInput.Axis.y);
        if (glm::length2(moveDirection) > 0.0001f)
            moveDirection = glm::normalize(moveDirection);

        const f32 speedFactor = moveInput.Sprint ? ctrl.SprintMultiplier : 1.0f;
        kcc.Move(moveDirection * speedFactor, moveInput.Sprint);
        if (moveInput.JumpPressed)
            kcc.Jump();

        if (ctrl.OrientRotationToMovement && glm::length2(moveDirection) > 0.001f)
        {
            const f32 targetAngle = std::atan2(moveDirection.x, moveDirection.z);
            const quat targetRotation = glm::angleAxis(targetAngle, vec3(0.0f, 1.0f, 0.0f));
            const f32 blend = glm::clamp(ctrl.RotationSmoothSpeed * dt, 0.0f, 1.0f);
            transform.rotation = glm::slerp(transform.rotation, targetRotation, blend);
        }
    }
}

void ThirdPersonControllerSystem::UpdateControllers(Scene& scene, f32 dt)
{
    auto& world = scene.GetRegistry();
    const auto& input = InputManager::Get();
    const auto defaultMoveInput = ReadMoveInput(input);

    auto view = world.view<ThirdPersonControllerComponent, CharacterControllerComponent, TransformComponent>();
    for (const auto entity : view)
    {
        auto& ctrl = view.get<ThirdPersonControllerComponent>(entity);
        const ThirdPersonMoveInput graphMoveInput{
            std::exchange(ctrl.GraphMoveAxis, vec2(0.0f)),
            std::exchange(ctrl.GraphSprint, false),
            std::exchange(ctrl.GraphJumpRequested, false)
        };
        if (!IsComponentEnabled<ThirdPersonControllerComponent, CharacterControllerComponent>(world, entity)) continue;

        const auto* flag = world.try_get<FlagComponent>(entity);
        if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) || HasFlag(flag->flags, EEntityFlag::Dead)))
            continue;

        if (!ctrl.InputEnabled) continue;

        auto* arm = FindSpringArm(world, entity, ctrl.SpringArmEntity);
        if (arm)
            UpdateOrbit(*arm, ctrl, input);

        auto& kcc = view.get<CharacterControllerComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);
        const auto& moveInput = ctrl.UseGraphMoveInput ? graphMoveInput : defaultMoveInput;
        ApplyMovement(kcc, transform, ctrl, moveInput, arm ? arm->Yaw : 0.0f, dt);
    }
}
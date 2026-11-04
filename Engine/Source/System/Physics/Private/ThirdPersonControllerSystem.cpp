#include "System/Physics/Public/ThirdPersonControllerSystem.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "Components/Physics/CharacterControllerComponent.h"
#include "Components/Physics/ThirdPersonControllerComponent.h"
#include "Components/Camera/SpringArmComponent.h"
#include "System/Input/Public/InputManager.h"
#include <glm/gtc/quaternion.hpp>
#include <algorithm>

namespace Engine
{
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

    void ThirdPersonControllerSystem::UpdateControllers(Scene& scene, f32 dt)
    {
        auto& world = scene.GetRegistry();
        auto& input = InputManager::Get();

        auto view = world.view<ThirdPersonControllerComponent, CharacterControllerComponent, TransformComponent>();
        for (auto entity : view)
        {
            const auto* flag = world.try_get<FlagComponent>(entity);
            if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) || HasFlag(flag->flags, EEntityFlag::Dead)))
                continue;

            auto& ctrl = view.get<ThirdPersonControllerComponent>(entity);
            auto& kcc = view.get<CharacterControllerComponent>(entity);
            auto& transform = view.get<TransformComponent>(entity);

            if (!ctrl.InputEnabled) continue;

            // 1. 연동된 SpringArmComponent 탐색 (지정 엔티티 -> 본인 -> 자식 순서)
            SpringArmComponent* arm = nullptr;
            if (ctrl.SpringArmEntity != entt::null && world.valid(ctrl.SpringArmEntity))
            {
                arm = world.try_get<SpringArmComponent>(ctrl.SpringArmEntity);
            }
            if (!arm)
            {
                arm = world.try_get<SpringArmComponent>(entity);
            }
            if (!arm)
            {
                if (const auto* hier = world.try_get<HierarchyComponent>(entity))
                {
                    for (auto child : hier->children)
                    {
                        if (world.valid(child) && (arm = world.try_get<SpringArmComponent>(child)))
                            break;
                    }
                }
            }

            // 2. 마우스 회전 및 줌 처리 (Spring Arm 오빗 회전)
            f32 armYaw = 0.0f;
            if (arm)
            {
                const bool canRotate = !ctrl.RequireRightClickToRotate || input.IsMouseButtonPressed(EMouseButton::Right);
                if (canRotate)
                {
                    const vec2 mouseDelta = input.GetMouseDelta();
                    if (glm::length(mouseDelta) > 0.0001f)
                    {
                        arm->Yaw += mouseDelta.x * ctrl.MouseSensitivity;
                        const f32 pitchSign = ctrl.InvertPitch ? 1.0f : -1.0f;
                        arm->Pitch += mouseDelta.y * ctrl.MouseSensitivity * pitchSign;
                        arm->Pitch = std::clamp(arm->Pitch, arm->MinPitch, arm->MaxPitch);
                    }
                }

                const f32 scrollY = input.GetMouseScrollDeltaY();
                if (std::abs(scrollY) > 0.001f)
                {
                    arm->TargetArmLength = std::clamp(arm->TargetArmLength - scrollY * ctrl.ZoomSensitivity, 0.5f, 50.0f);
                }

                armYaw = arm->Yaw;
            }

            // 3. 카메라 수평 회전(Yaw) 기준 전방/우측 벡터 계산
            const quat yawRot = quat(vec3(0.0f, glm::radians(armYaw), 0.0f));
            const vec3 camForward = glm::normalize(yawRot * vec3(0.0f, 0.0f, 1.0f));
            const vec3 camRight = glm::normalize(yawRot * vec3(1.0f, 0.0f, 0.0f));

            // 4. 키보드 WASD 입력 수집 및 이동 벡터 산출
            vec3 moveDir = vec3(0.0f);
            if (input.IsKeyButtonPressed(EKeyCode::W)) moveDir += camForward;
            if (input.IsKeyButtonPressed(EKeyCode::S)) moveDir -= camForward;
            if (input.IsKeyButtonPressed(EKeyCode::D)) moveDir += camRight;
            if (input.IsKeyButtonPressed(EKeyCode::A)) moveDir -= camRight;

            if (glm::length2(moveDir) > 0.0001f)
                moveDir = glm::normalize(moveDir);

            // 5. 달리기 배율 적용 및 KCC 이동 요청
            const bool isSprinting = input.IsShiftPressed();
            const f32 speedFactor = isSprinting ? ctrl.SprintMultiplier : 1.0f;
            kcc.Move(moveDir * speedFactor);

            // 6. 점프 처리 (스페이스바)
            if (input.IsKeyButtonDown(EKeyCode::Space))
            {
                kcc.Jump();
            }

            // 7. 캐릭터 이동 방향으로 부드러운 몸체 회전 (Orient Rotation to Movement)
            if (ctrl.OrientRotationToMovement && glm::length2(moveDir) > 0.001f)
            {
                const f32 targetAngle = std::atan2(moveDir.x, moveDir.z);
                const quat targetRot = glm::angleAxis(targetAngle, vec3(0.0f, 1.0f, 0.0f));
                transform.rotation = glm::slerp(transform.rotation, targetRot, glm::clamp(ctrl.RotationSmoothSpeed * dt, 0.0f, 1.0f));
            }
        }
    }
}
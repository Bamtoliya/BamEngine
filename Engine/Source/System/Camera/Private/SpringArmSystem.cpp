#include "SpringArmSystem.h"
#include "Scene.h"
#include "PhysicsSystem.h"
#include "TransformSystem.h"
#include "Components/Camera/SpringArmComponent.h"
#include "Components/Render/CameraComponent.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Core/HierarchyComponent.h"
#include "Components/Core/FlagComponent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>

namespace Engine
{
    EResult SpringArmSystem::Initialize(void* arg)
    {
        (void)arg;
        return EResult::Success;
    }

    SpringArmSystem* SpringArmSystem::Create(void* arg)
    {
        auto* system = new SpringArmSystem();
        if (system->Initialize(arg) != EResult::Success)
        {
            Safe_Release(system);
            return nullptr;
        }
        return system;
    }

    void SpringArmSystem::Free()
    {
    }

    void SpringArmSystem::OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt)
    {
        (void)globalRegistry;
        if (!std::isfinite(dt) || dt <= 0.0f) return;

        for (Scene* scene : activeScenes)
        {
            if (scene && scene->IsActive() && !scene->IsPaused())
                UpdateSpringArms(*scene, dt);
        }
    }

    void SpringArmSystem::UpdateSpringArms(Scene& scene, f32 dt)
    {
        auto& world = scene.GetRegistry();
        auto view = world.view<SpringArmComponent, TransformComponent, WorldTransformComponent>();

        for (auto entity : view)
        {
            const auto* flag = world.try_get<FlagComponent>(entity);
            if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) || HasFlag(flag->flags, EEntityFlag::Dead)))
                continue;

            auto& arm = view.get<SpringArmComponent>(entity);
            const auto* hierarchy = world.try_get<HierarchyComponent>(entity);

            // 1. 타깃 기준점 위치 및 회전 산출
            vec3 targetWorldPos = vec3(0.0f);
            entt::entity ignoreEntity = entity;

            if (arm.TargetEntity != entt::null && world.valid(arm.TargetEntity))
            {
                // 1-A. 명시적으로 타깃 엔티티가 지정된 경우 (예: 플레이어 엔티티)
                const auto* targetWorld = world.try_get<WorldTransformComponent>(arm.TargetEntity);
                targetWorldPos = (targetWorld ? vec3(targetWorld->worldMatrix[3]) : vec3(0.0f)) + arm.TargetOffset;
                ignoreEntity = arm.TargetEntity;
            }
            else if (hierarchy && hierarchy->parent != entt::null && world.valid(hierarchy->parent))
            {
                // 1-B. 부모가 있는 경우 (예: 캐릭터의 자식으로 스프링 암이 부착된 경우)
                const auto& parentWorld = world.get<WorldTransformComponent>(hierarchy->parent);
                targetWorldPos = vec3(parentWorld.worldMatrix[3]) + arm.TargetOffset;
                ignoreEntity = hierarchy->parent;
            }
            else
            {
                // 1-C. 단독 빈 엔티티인 경우:
                // 매 틱 뒤로 누적되지 않도록 최초 위치를 앵커로 고정
                if (!arm.HasAnchor)
                {
                    const auto& transform = view.get<TransformComponent>(entity);
                    arm.AnchorPosition = transform.position;
                    arm.HasAnchor = true;
                }
                targetWorldPos = arm.AnchorPosition + arm.TargetOffset;
            }

            // 2. 스프링 암 회전 (Pitch, Yaw) 계산
            const quat armRot = quat(vec3(glm::radians(arm.Pitch), glm::radians(arm.Yaw), 0.0f));
            const vec3 forward = armRot * vec3(0.0f, 0.0f, 1.0f);
            const vec3 armDir = -forward; // 후방 방향

            // 3. 벽/장애물 충돌 검사 (Collision Probe)
            f32 targetDistance = arm.TargetArmLength;

            if (arm.DoCollisionTest && arm.TargetArmLength > 0.01f)
            {
                Ray probeRay(targetWorldPos, armDir);
                RaycastQuery query{};
                query.maxDistance = arm.TargetArmLength;
                query.layerMask = arm.CollisionMask;
                query.includeTrigger = false;
                query.ignoreEntity = ignoreEntity;

                SceneRaycastHit hit{};
                if (PhysicsSystem::Raycast(scene, probeRay, hit, query) && hit.Hit.hasHit)
                {
                    targetDistance = glm::max(0.1f, hit.Hit.distance - arm.ProbeRadius);
                }
            }

            // 장애물에 부딪혀 거리가 줄어들 때는 즉시 당기고, 넓어질 때는 부드럽게 복귀
            if (targetDistance < arm.CurrentArmLength)
            {
                arm.CurrentArmLength = targetDistance;
            }
            else
            {
                arm.CurrentArmLength = glm::mix(arm.CurrentArmLength, targetDistance, glm::clamp(arm.CameraLagSpeed * dt, 0.0f, 1.0f));
            }

            // 4. 최종 카메라 위치 산출 (암 길이 + 소켓 오프셋)
            const vec3 socketOffsetWorld = armRot * arm.SocketOffset;
            const vec3 desiredPos = targetWorldPos + armDir * arm.CurrentArmLength + socketOffsetWorld;

            // 5. 카메라 지연 (Camera Lag) 보간
            vec3 finalCameraPos = desiredPos;
            if (arm.EnableCameraLag && arm.IsInitialized)
            {
                arm.PreviousDesiredPosition = glm::mix(arm.PreviousDesiredPosition, desiredPos, glm::clamp(arm.CameraLagSpeed * dt, 0.0f, 1.0f));
                finalCameraPos = arm.PreviousDesiredPosition;
            }
            else
            {
                arm.PreviousDesiredPosition = desiredPos;
                arm.IsInitialized = true;
            }

            // 6. 자식 카메라 엔티티 탐색 (자식에 CameraComponent가 있으면 자식을 이동, 없으면 현재 엔티티를 이동)
            entt::entity cameraEntity = entt::null;
            if (hierarchy && !hierarchy->children.empty())
            {
                for (entt::entity child : hierarchy->children)
                {
                    if (world.all_of<CameraComponent>(child))
                    {
                        cameraEntity = child;
                        break;
                    }
                }
            }
            entt::entity targetCamEntity = (cameraEntity != entt::null && world.valid(cameraEntity)) ? cameraEntity : entity;
            auto& camTransform = world.get<TransformComponent>(targetCamEntity);
            auto& camWorld = world.get<WorldTransformComponent>(targetCamEntity);
            // 부모 계층이 있는 경우 월드 좌표를 부모 기준 로컬 좌표로 변환하여 적용
            const auto* camHier = world.try_get<HierarchyComponent>(targetCamEntity);
            if (camHier && camHier->parent != entt::null && world.valid(camHier->parent))
            {
                if (const auto* parentWorld = world.try_get<WorldTransformComponent>(camHier->parent))
                {
                    const mat4 invParent = glm::inverse(parentWorld->worldMatrix);
                    const vec3 localPos = vec3(invParent * vec4(finalCameraPos, 1.0f));
                    const quat localRot = glm::quat_cast(invParent * glm::mat4_cast(armRot));
                    std::ignore = camTransform.SetLocalPosition(localPos);
                    camTransform.rotation = localRot;
                    camTransform.SynchronizeRotation();

                    glm::mat4 composedWorld;
                    if (TransformSystem::TryComposeWorld(camTransform, parentWorld->worldMatrix, composedWorld))
                    {
                        TransformSystem::UpdateWorld(camTransform, parentWorld->worldMatrix, camWorld);
                    }
                }
            }
            else
            {
                std::ignore = camTransform.SetLocalPosition(finalCameraPos);
                camTransform.rotation = armRot;
                TransformSystem::UpdateRootWorld(camTransform, camWorld);
            }
        }
    }
}
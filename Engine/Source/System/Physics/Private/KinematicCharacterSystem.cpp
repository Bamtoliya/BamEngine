#include "KinematicCharacterSystem.h"
#include "Scene.h"
#include "Collision.h"
#include "PhysicsSystem.h"
#include "TransformSystem.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Components/Core/HierarchyComponent.h"
#include "Components/Physics/ColliderComponent.h"
#include "Mesh.h"
#include "Components/Render/TerrainComponent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include "Components/Render/AnimatorComponent.h"

namespace
{
    AnimatorComponent* FindAnimatorInHierarchy(entt::registry& world, entt::entity root)
    {
        const auto* hierarchy = world.try_get<HierarchyComponent>(root);
        if (!hierarchy) return nullptr;
        for (entt::entity child : hierarchy->children)
        {
            if (!world.valid(child)) continue;
            if (auto* anim = world.try_get<AnimatorComponent>(child)) return anim;
            if (auto* childAnim = FindAnimatorInHierarchy(world, child)) return childAnim;
        }
        return nullptr;
    }
}

namespace Engine
{
    EResult KinematicCharacterSystem::Initialize(void* arg)
    {
        return EResult::Success;
    }

    KinematicCharacterSystem* KinematicCharacterSystem::Create(void* arg)
    {
        auto* system = new KinematicCharacterSystem();
        if (system->Initialize(arg) != EResult::Success)
        {
            Safe_Release(system);
            return nullptr;
        }
        return system;
    }

    void KinematicCharacterSystem::Free()
    {
    }

    bool KinematicCharacterSystem::SweepCharacter(const Scene& scene, const CharacterControllerComponent& kcc,
        const vec3& fromPos, const vec3& direction, f32 distance, HitResult& outHit, entt::entity ignoreEntity)
    {
        outHit.Reset();
        if (distance <= 1.0e-5f || !std::isfinite(distance)) return false;

        const auto& world = scene.GetRegistry();
        BoundingBox charBox = kcc.GetWorldAABB(fromPos);

        // 수평 이동 시 발바닥이 딛고 있는 지면 AABB와 스쳐서 멈추지 않도록 하단을 살짝 띄움
        if (std::abs(direction.y) < 0.1f)
        {
            charBox.min.y += glm::min(kcc.SkinWidth + 0.02f, kcc.Height * 0.25f);
        }

        f32 closestDist = distance;
        bool hasHit = false;

        auto view = world.view<ColliderComponent, WorldTransformComponent>();
        for (auto entity : view)
        {
            if (entity == ignoreEntity) continue;
            const auto& collider = view.get<ColliderComponent>(entity);
            const auto& settings = collider.GetSettings();
            if (!settings.Enabled || settings.IsTrigger || (settings.CollisionLayer & kcc.CollisionMask) == 0u) continue;

            const auto* flag = world.try_get<FlagComponent>(entity);
            if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
                HasFlag(flag->flags, EEntityFlag::Dead))) continue;

            const auto& worldTransform = view.get<WorldTransformComponent>(entity);
            const mat4 colliderWorld = worldTransform.worldMatrix * glm::translate(mat4(1.0f), collider.GetOffset());

            if (const auto* box = std::get_if<BoxShape>(&collider.GetShape()))
            {
                if (!box->HasValidParameters()) continue;
                const vec3 halfSize = box->Size * 0.5f;
                const vec3 worldCenter = vec3(colliderWorld[3]);
                const vec3 extent = glm::abs(vec3(colliderWorld[0])) * halfSize.x + glm::abs(vec3(colliderWorld[1])) * halfSize.y + glm::abs(vec3(colliderWorld[2])) * halfSize.z;
                const BoundingBox obstacle(worldCenter - extent, worldCenter + extent);

                HitResult tempHit{};
                if (Collision::Sweep(charBox, direction, closestDist, obstacle, tempHit) &&
                    tempHit.hasHit && tempHit.distance < closestDist)
                {
                    closestDist = tempHit.distance;
                    outHit = tempHit;
                    hasHit = true;
                }
            }
            else if (const auto* sphere = std::get_if<SphereShape>(&collider.GetShape()))
            {
                if (!sphere->HasValidParameters()) continue;
                const f32 maxScale = glm::max(glm::length(vec3(colliderWorld[0])), glm::max(glm::length(vec3(colliderWorld[1])), glm::length(vec3(colliderWorld[2]))));
                const vec3 worldCenter = vec3(colliderWorld[3]);
                const BoundingSphere obstacle(worldCenter, sphere->Radius * maxScale);
                HitResult tempHit{};
                const Capsule charCap = kcc.GetWorldCapsule(fromPos);
                if (Collision::Sweep(charCap, direction, closestDist, obstacle, tempHit) &&
                    tempHit.hasHit && tempHit.distance < closestDist)
                {
                    closestDist = tempHit.distance;
                    outHit = tempHit;
                    hasHit = true;
                }
            }
            else if (const auto* capsule = std::get_if<CapsuleShape>(&collider.GetShape()))
            {
                if (!capsule->HasValidParameters()) continue;
                const f32 halfSegment = std::max(0.0f, (capsule->Height * 0.5f) - capsule->Radius);
                const f32 scaleXZ = glm::max(glm::length(vec3(colliderWorld[0])), glm::length(vec3(colliderWorld[2])));
                const vec3 pA = vec3(colliderWorld * vec4(0.0f, -halfSegment, 0.0f, 1.0f));
                const vec3 pB = vec3(colliderWorld * vec4(0.0f, halfSegment, 0.0f, 1.0f));
                const Capsule obstacleCap(pA, pB, capsule->Radius * scaleXZ);
                HitResult tempHit{};
                const Capsule charCap = kcc.GetWorldCapsule(fromPos);
                if (Collision::Sweep(charCap, direction, closestDist, obstacleCap, tempHit) &&
                    tempHit.hasHit && tempHit.distance < closestDist)
                {
                    closestDist = tempHit.distance;
                    outHit = tempHit;
                    hasHit = true;
                }
            }
            else if (const auto* meshShape = std::get_if<MeshShape>(&collider.GetShape()))
            {
                if (meshShape->HasMesh())
                {
                    if (auto* mesh = meshShape->MeshHandle.Get())
                    {
                        HitResult tempHit{};
                        const vec3 bottomCenter = fromPos + kcc.Center - vec3(0.0f, kcc.Height * 0.5f - kcc.Radius, 0.0f);
                        const Ray sweepRay(bottomCenter, direction);
                        if (Collision::Raycast(sweepRay, *mesh, colliderWorld, tempHit, closestDist) &&
                            tempHit.hasHit && tempHit.distance < closestDist)
                        {
                            closestDist = tempHit.distance;
                            outHit = tempHit;
                            hasHit = true;
                        }
                    }
                }
            }
        }

        return hasHit;
    }

    bool KinematicCharacterSystem::DetectGround(const Scene& scene, const CharacterControllerComponent& kcc,
        const vec3& currentPos, HitResult& outGroundHit, entt::entity ignoreEntity, entt::entity* outHitEntity)
    {
        outGroundHit.Reset();
        if (outHitEntity) *outHitEntity = entt::null;

        const vec3 bottomSphere = currentPos + kcc.Center - vec3(0.0f, kcc.Height * 0.5f - kcc.Radius, 0.0f);
        const f32 snapMargin = kcc.IsGrounded ? 0.15f : 0.02f;
        const f32 checkDist = kcc.Radius + kcc.SkinWidth + snapMargin;
        Ray downRay(bottomSphere, vec3(0.0f, -1.0f, 0.0f));
        RaycastQuery query{};
        query.maxDistance = checkDist;
        query.layerMask = kcc.CollisionMask;
        query.includeTrigger = false;
        query.ignoreEntity = ignoreEntity;

        SceneRaycastHit sceneHit{};
        if (PhysicsSystem::Raycast(scene, downRay, sceneHit, query) && sceneHit.Hit.hasHit)
        {
            outGroundHit = sceneHit.Hit;
            if (outHitEntity) *outHitEntity = sceneHit.Entity;
            return true;
        }

        return SweepCharacter(scene, kcc, currentPos, vec3(0.0f, -1.0f, 0.0f), checkDist, outGroundHit, ignoreEntity);
    }

    void KinematicCharacterSystem::UpdateCharacter(Scene& scene, entt::entity entity, CharacterControllerComponent& kcc, f32 dt)
    {
        auto& world = scene.GetRegistry();
        auto& transform = world.get<TransformComponent>(entity);
        vec3 currentPos = transform.position;
        const f32 footOffset = kcc.Center.y - (kcc.Height * 0.5f);

        // 0. 무빙 플랫폼(Moving Platform) 위치/회전 상속
        if (kcc.IsGrounded && kcc.GroundEntity != entt::null && world.valid(kcc.GroundEntity))
        {
            if (const auto* groundTransform = world.try_get<TransformComponent>(kcc.GroundEntity))
            {
                const vec3 groundPos = groundTransform->position;
                const quat groundRot = groundTransform->rotation;

                // 위치 이동 상속
                const vec3 deltaPlatformPos = groundPos - kcc.LastGroundPos;
                currentPos += deltaPlatformPos;

                // 플랫폼 중심 기준 회전 상속
                const quat deltaPlatformRot = groundRot * glm::inverse(kcc.LastGroundRot);
                if (glm::length2(deltaPlatformRot - quat(1.0f, 0.0f, 0.0f, 0.0f)) > 1.0e-6f)
                {
                    const vec3 offsetFromCenter = currentPos - groundPos;
                    const vec3 rotatedOffset = deltaPlatformRot * offsetFromCenter;
                    currentPos = groundPos + rotatedOffset;

                    transform.rotation = glm::normalize(deltaPlatformRot * transform.rotation);
                    std::ignore = transform.SetEulerRotation(glm::degrees(glm::eulerAngles(transform.rotation)));
                }

                kcc.LastGroundPos = groundPos;
                kcc.LastGroundRot = groundRot;
            }
            else
            {
                kcc.GroundEntity = entt::null;
            }
        }
        else
        {
            kcc.GroundEntity = entt::null;
        }

        // 1. 이미 착지 상태일 때 발바닥 스냅 (Ground Snapping)
        if (kcc.IsGrounded)
        {
            HitResult groundHit{};
            entt::entity hitEntity = entt::null;
            const bool foundGround = DetectGround(scene, kcc, currentPos, groundHit, entity, &hitEntity);
            if (foundGround && groundHit.normal.y > 0.0f)
            {
                const f32 cosAngle = glm::clamp(glm::dot(groundHit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
                const f32 slopeAngle = glm::degrees(std::acos(cosAngle));
                if (slopeAngle <= kcc.SlopeLimitDegrees)
                {
                    currentPos.y = groundHit.point.y - footOffset;
                    kcc.Velocity.y = 0.0f;
                    kcc.IsGrounded = true;
                    kcc.GroundNormal = groundHit.normal;
                    kcc.GroundAngle = slopeAngle;
                    if (hitEntity != entt::null)
                    {
                        kcc.GroundEntity = hitEntity;
                        if (const auto* gt = world.try_get<TransformComponent>(hitEntity))
                        {
                            kcc.LastGroundPos = gt->position;
                            kcc.LastGroundRot = gt->rotation;
                        }
                    }
                }
                else
                {
                    kcc.IsGrounded = false;
                    kcc.GroundEntity = entt::null;
                }
            }
            else
            {
                kcc.IsGrounded = false;
                kcc.GroundEntity = entt::null;
            }
        }

        // 2. 점프 처리
        bool justJumped = false;
        if (kcc.JumpRequested)
        {
            if (kcc.IsGrounded)
            {
                kcc.Velocity.y = kcc.JumpForce;
                kcc.IsGrounded = false;
                kcc.GroundEntity = entt::null;
                justJumped = true;
            }
            kcc.JumpRequested = false;
        }

        // 3. 중력 가속도 적용 (체공 중일 때만)
        if (!kcc.IsGrounded)
        {
            kcc.Velocity.y += kcc.Gravity * dt;
        }

        // 4. 수평 이동 목표 속도 산출
        vec3 inputDir = kcc.MoveInput;
        if (glm::length2(inputDir) > 1.0e-4f)
        {
            inputDir = glm::normalize(vec3(inputDir.x, 0.0f, inputDir.z));
        }
        else
        {
            inputDir = vec3(0.0f);
        }

        const f32 targetSpeed = kcc.IsRunning ? kcc.RunSpeed : kcc.WalkSpeed;
        const vec3 desiredHorizVel = inputDir * targetSpeed;

        if (kcc.IsGrounded)
        {
            kcc.Velocity.x = desiredHorizVel.x;
            kcc.Velocity.z = desiredHorizVel.z;
        }
        else
        {
            // 체공 중 AirControl 보간
            kcc.Velocity.x = glm::mix(kcc.Velocity.x, desiredHorizVel.x, glm::clamp(kcc.AirControl * 10.0f * dt, 0.0f, 1.0f));
            kcc.Velocity.z = glm::mix(kcc.Velocity.z, desiredHorizVel.z, glm::clamp(kcc.AirControl * 10.0f * dt, 0.0f, 1.0f));
        }

        // 5. 수평 이동 및 장애물 슬라이딩 (Slide & Step-up)
        vec3 horizMove = vec3(kcc.Velocity.x, 0.0f, kcc.Velocity.z) * dt;
        f32 horizDist = glm::length(horizMove);

        if (horizDist > 1.0e-5f)
        {
            vec3 moveDir = horizMove / horizDist;
            f32 remainingDist = horizDist;
            const int maxBounces = 3;

            for (int bounce = 0; bounce < maxBounces && remainingDist > 1.0e-5f; ++bounce)
            {
                HitResult hit{};
                if (SweepCharacter(scene, kcc, currentPos, moveDir, remainingDist, hit, entity))
                {
                    const f32 travelDist = glm::max(0.0f, hit.distance - kcc.SkinWidth);
                    currentPos += moveDir * travelDist;
                    remainingDist -= travelDist;

                    // 계단/턱 오르기 (Step-Up)
                    bool stepped = false;
                    if (kcc.IsGrounded && kcc.StepOffset > 0.001f)
                    {
                        const vec3 stepUpPos = currentPos + vec3(0.0f, kcc.StepOffset, 0.0f);
                        HitResult stepHit{};
                        if (!SweepCharacter(scene, kcc, stepUpPos, moveDir, remainingDist, stepHit, entity))
                        {
                            HitResult groundHit{};
                            const vec3 forwardPos = stepUpPos + moveDir * remainingDist;
                            if (SweepCharacter(scene, kcc, forwardPos, vec3(0.0f, -1.0f, 0.0f), kcc.StepOffset + kcc.SkinWidth, groundHit, entity))
                            {
                                currentPos = forwardPos;
                                currentPos.y = groundHit.point.y - footOffset;
                                stepped = true;
                                remainingDist = 0.0f;
                            }
                        }
                    }

                    if (!stepped)
                    {
                        // 벽 슬라이딩 (법선 평면 투영)
                        vec3 wallNormal = hit.normal;
                        wallNormal.y = 0.0f;
                        if (glm::length2(wallNormal) > 1.0e-4f)
                        {
                            wallNormal = glm::normalize(wallNormal);
                            moveDir = moveDir - wallNormal * glm::dot(moveDir, wallNormal);
                            if (glm::length2(moveDir) > 1.0e-4f)
                            {
                                moveDir = glm::normalize(moveDir);
                            }
                            else
                            {
                                break;
                            }
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                else
                {
                    currentPos += moveDir * remainingDist;
                    remainingDist = 0.0f;
                }
            }
        }

        // 6. 수직 이동 및 천장/바닥 충돌
        if (!kcc.IsGrounded)
        {
            const f32 vertDelta = kcc.Velocity.y * dt;
            if (vertDelta > 0.0f)
            {
                HitResult ceilingHit{};
                if (SweepCharacter(scene, kcc, currentPos, vec3(0.0f, 1.0f, 0.0f), vertDelta, ceilingHit, entity))
                {
                    currentPos.y += glm::max(0.0f, ceilingHit.distance - kcc.SkinWidth);
                    kcc.Velocity.y = 0.0f;
                }
                else
                {
                    currentPos.y += vertDelta;
                }
            }
            else
            {
                HitResult fallHit{};
                const f32 fallDist = -vertDelta;
                if (SweepCharacter(scene, kcc, currentPos, vec3(0.0f, -1.0f, 0.0f), fallDist, fallHit, entity))
                {
                    currentPos.y = fallHit.point.y - footOffset;
                    kcc.Velocity.y = 0.0f;
                    kcc.IsGrounded = true;
                    kcc.GroundNormal = fallHit.normal;

                    // 착지 시 지면 엔티티 탐색
                    entt::entity landingEntity = entt::null;
                    HitResult landingHit{};
                    if (DetectGround(scene, kcc, currentPos, landingHit, entity, &landingEntity) && landingEntity != entt::null)
                    {
                        kcc.GroundEntity = landingEntity;
                        if (const auto* gt = world.try_get<TransformComponent>(landingEntity))
                        {
                            kcc.LastGroundPos = gt->position;
                            kcc.LastGroundRot = gt->rotation;
                        }
                    }
                }
                else
                {
                    currentPos.y += vertDelta;
                }
            }
        }

        // 7. 위치 반영 및 입력 초기화
        std::ignore = transform.SetLocalPosition(currentPos);
        auto& worldTransform = world.get<WorldTransformComponent>(entity);
        TransformSystem::UpdateRootWorld(transform, worldTransform);
        kcc.MoveInput = vec3(0.0f);

        // 8. 애니메이터 파라미터 자동 주입 (Locomotion)
        if (kcc.AutoUpdateAnimator)
        {
            AnimatorComponent* animator = nullptr;
            if (kcc.TargetAnimatorEntity != entt::null && world.valid(kcc.TargetAnimatorEntity))
            {
                animator = world.try_get<AnimatorComponent>(kcc.TargetAnimatorEntity);
            }
            if (!animator)
            {
                animator = world.try_get<AnimatorComponent>(entity);
            }
            if (!animator)
            {
                animator = FindAnimatorInHierarchy(world, entity);
            }

            if (animator)
            {
                const f32 horizontalSpeed = glm::length(vec2(kcc.Velocity.x, kcc.Velocity.z));
                if (!kcc.SpeedParamName.empty())
                {
                    animator->SetFloat(kcc.SpeedParamName, horizontalSpeed);
                }
                if (!kcc.IsGroundedParamName.empty())
                {
                    animator->SetBool(kcc.IsGroundedParamName, kcc.IsGrounded);
                }
                if (justJumped && !kcc.JumpParamName.empty())
                {
                    animator->SetTrigger(kcc.JumpParamName);
                }

                if (kcc.ScalePlayRateWithSpeed && horizontalSpeed > 0.1f)
                {
                    const f32 baseSpeed = kcc.IsRunning ? kcc.RunSpeed : kcc.WalkSpeed;
                    animator->playRate = std::clamp(horizontalSpeed / std::max(baseSpeed, 0.1f), kcc.MinPlayRate, kcc.MaxPlayRate);
                }
                else
                {
                    animator->playRate = 1.0f;
                }
            }
        }
    }

    void KinematicCharacterSystem::OnFixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
    {
        (void)registry;
        if (!std::isfinite(dt) || dt <= 0.0f) return;

        for (Scene* scene : activeScenes)
        {
            if (!scene || !scene->IsActive() || scene->IsPaused()) continue;

            auto& world = scene->GetRegistry();
            auto view = world.view<CharacterControllerComponent, TransformComponent, WorldTransformComponent>();

            for (auto entity : view)
            {
                const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
                if (hierarchy && hierarchy->parent != entt::null) continue;

                const auto* flag = world.try_get<FlagComponent>(entity);
                if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) || HasFlag(flag->flags, EEntityFlag::Dead))) continue;

                auto& kcc = view.get<CharacterControllerComponent>(entity);
                UpdateCharacter(*scene, entity, kcc, dt);
            }
        }
    }
}

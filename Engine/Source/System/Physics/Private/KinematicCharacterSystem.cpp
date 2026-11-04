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
#include "Components/Render/TerrainComponent.h"
#include <glm/gtc/matrix_transform.hpp>
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
                const BoundingBox localBox(-halfSize, halfSize);
                BoundingBox obstacle{};
                if (!localBox.TryTransformToAABB(colliderWorld, obstacle)) continue;

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
                const f32 maxScale = glm::max(glm::max(glm::length(vec3(colliderWorld[0])),
                    glm::length(vec3(colliderWorld[1]))), glm::length(vec3(colliderWorld[2])));
                const vec3 worldCenter = vec3(colliderWorld[3]);
                const BoundingSphere obstacle(worldCenter, sphere->Radius * maxScale);

                HitResult tempHit{};
                if (Collision::Sweep(charBox, direction, closestDist, obstacle, tempHit) &&
                    tempHit.hasHit && tempHit.distance < closestDist)
                {
                    closestDist = tempHit.distance;
                    outHit = tempHit;
                    hasHit = true;
                }
            }
        }

        // 지형(TerrainComponent) 레이캐스트 충돌 검사
        auto terrainView = world.view<TerrainComponent, WorldTransformComponent>();
        for (auto entity : terrainView)
        {
            if (entity == ignoreEntity) continue;
            const auto& terrain = terrainView.get<TerrainComponent>(entity);
            const auto& worldTransform = terrainView.get<WorldTransformComponent>(entity);

            const vec3 bottomCenter = fromPos + kcc.Center - vec3(0.0f, kcc.Height * 0.5f - kcc.Radius, 0.0f);
            Ray sweepRay(bottomCenter, direction);
            HitResult terrainHit{};
            if (Collision::Raycast(sweepRay, terrain, worldTransform.worldMatrix, terrainHit, closestDist) &&
                terrainHit.hasHit && terrainHit.distance < closestDist)
            {
                closestDist = terrainHit.distance;
                outHit = terrainHit;
                hasHit = true;
            }
        }

        return hasHit;
    }

    bool KinematicCharacterSystem::DetectGround(const Scene& scene, const CharacterControllerComponent& kcc, const vec3& currentPos, HitResult& outGroundHit, entt::entity ignoreEntity)
    {
        outGroundHit.Reset();
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

        // 1. 이미 땅을 밟고 있을 때만 지면 굴곡을 추적하여 발바닥 높이 유지 (Ground Snapping)
        if (kcc.IsGrounded)
        {
            HitResult groundHit{};
            const bool foundGround = DetectGround(scene, kcc, currentPos, groundHit, entity);
            if (foundGround && groundHit.normal.y > 0.0f)
            {
                const f32 cosAngle = glm::clamp(glm::dot(groundHit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
                const f32 slopeAngle = glm::degrees(std::acos(cosAngle));
                if (slopeAngle <= kcc.SlopeLimitDegrees)
                {
                    kcc.IsGrounded = true;
                    kcc.GroundNormal = groundHit.normal;
                    kcc.GroundAngle = slopeAngle;
                    currentPos.y = groundHit.point.y - footOffset;
                }
                else
                {
                    kcc.IsGrounded = false;
                    const vec3 slideDir = glm::normalize(vec3(groundHit.normal.x, 0.0f, groundHit.normal.z));
                    kcc.Velocity += slideDir * 9.81f * dt;
                }
            }
            else
            {
                // 지면이 끊기면 공중(낙하) 상태로 자연스럽게 전환
                kcc.IsGrounded = false;
                kcc.GroundNormal = vec3(0.0f, 1.0f, 0.0f);
                kcc.GroundAngle = 0.0f;
            }
        }

        // 2. 점프 처리 (Jump Execution)
        bool justJumped = false;
        if (kcc.JumpRequested && kcc.IsGrounded)
        {
            kcc.Velocity.y = kcc.JumpForce;
            kcc.IsGrounded = false;
            kcc.JumpRequested = false;
            justJumped = true;
        }
        else
        {
            kcc.JumpRequested = false;
        }

        // 3. 중력 적용 (Gravity)
        if (!kcc.IsGrounded)
        {
            kcc.Velocity.y += kcc.Gravity * dt;
            kcc.Velocity.y = glm::max(kcc.Velocity.y, -50.0f);
        }

        // 4. 수평 입력 속도 보간 (Desired Velocity)
        vec3 inputDir = kcc.MoveInput;
        const f32 inputLen = glm::length(inputDir);
        if (inputLen > 1.0f) inputDir /= inputLen;

        const f32 targetSpeed = kcc.IsRunning ? kcc.RunSpeed : kcc.WalkSpeed;
        const vec3 targetHorizVel = inputDir * targetSpeed;
        const f32 control = kcc.IsGrounded ? 1.0f : kcc.AirControl;
        kcc.Velocity.x = glm::mix(kcc.Velocity.x, targetHorizVel.x, glm::clamp(15.0f * control * dt, 0.0f, 1.0f));
        kcc.Velocity.z = glm::mix(kcc.Velocity.z, targetHorizVel.z, glm::clamp(15.0f * control * dt, 0.0f, 1.0f));

        // 5. 수평 이동 및 장애물 슬라이딩 (Sweep & Slide with Step-Up)
        vec3 horizMove = vec3(kcc.Velocity.x, 0.0f, kcc.Velocity.z) * dt;
        if (kcc.IsGrounded && kcc.GroundAngle > 0.5f)
        {
            horizMove = horizMove - glm::dot(horizMove, kcc.GroundNormal) * kcc.GroundNormal;
        }

        vec3 remainingMove = horizMove;
        kcc.IsStepping = false;

        for (int iter = 0; iter < 3 && glm::length(remainingMove) > 1.0e-4f; ++iter)
        {
            const f32 moveDist = glm::length(remainingMove);
            const vec3 moveDir = remainingMove / moveDist;
            HitResult hit{};

            if (SweepCharacter(scene, kcc, currentPos, moveDir, moveDist, hit, entity))
            {
                bool stepped = false;
                // 계단/턱 오르기 (Step-Up)
                if (kcc.IsGrounded && kcc.StepOffset > 0.01f && hit.normal.y < 0.3f)
                {
                    const vec3 stepUpPos = currentPos + vec3(0.0f, kcc.StepOffset, 0.0f);
                    HitResult upHit{};
                    if (!SweepCharacter(scene, kcc, currentPos, vec3(0.0f, 1.0f, 0.0f), kcc.StepOffset, upHit, entity))
                    {
                        const f32 fwdDist = glm::min(moveDist, 0.2f);
                        HitResult fwdHit{};
                        if (!SweepCharacter(scene, kcc, stepUpPos, moveDir, fwdDist, fwdHit, entity))
                        {
                            const vec3 fwdPos = stepUpPos + moveDir * fwdDist;
                            HitResult downHit{};
                            if (SweepCharacter(scene, kcc, fwdPos, vec3(0.0f, -1.0f, 0.0f), kcc.StepOffset + 0.05f, downHit, entity))
                            {
                                currentPos = fwdPos - vec3(0.0f, downHit.distance, 0.0f);
                                remainingMove = vec3(0.0f);
                                stepped = true;
                                kcc.IsStepping = true;
                            }
                        }
                    }
                }

                if (!stepped)
                {
                    const f32 safeDist = glm::max(0.0f, hit.distance - kcc.SkinWidth);
                    currentPos += moveDir * safeDist;
                    remainingMove = remainingMove - moveDir * hit.distance;
                    remainingMove = remainingMove - glm::dot(remainingMove, hit.normal) * hit.normal;
                }
            }
            else
            {
                currentPos += remainingMove;
                break;
            }
        }

        // 6. 수직 이동 적용 (Vertical Movement)
        if (std::abs(kcc.Velocity.y) > 1.0e-4f)
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
                // 공중에서 떨어지다가 바닥과 실제로 닿는 순간에만 착지!
                if (SweepCharacter(scene, kcc, currentPos, vec3(0.0f, -1.0f, 0.0f), fallDist, fallHit, entity))
                {
                    currentPos.y = fallHit.point.y - footOffset;
                    kcc.Velocity.y = 0.0f;
                    kcc.IsGrounded = true;
                    kcc.GroundNormal = fallHit.normal;
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

                // [추가]: 실제 이동 속도에 비례하여 발걸음 재생 속도 동적 조절 (발 미끄러짐 및 헛발질 완전 제거)
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

    void KinematicCharacterSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
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
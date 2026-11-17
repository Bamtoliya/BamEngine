#include "KinematicCharacterSystem.h"
#include "Scene.h"
#include "Collision.h"
#include "PhysicsSystem.h"
#include "TransformSystem.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "HierarchyComponent.h"
#include "ColliderComponent.h"
#include "Mesh.h"
#include "TerrainComponent.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>
#include <algorithm>
#include "AnimatorComponent.h"

namespace
{
    f32 CalculateGroundContactY(const vec3& currentPos, f32 footOffset, f32 radius, const vec3& hitPoint, const vec3& hitNormal)
    {
        if (hitNormal.y > 1.0e-4f)
        {
            const vec3 C = currentPos + vec3(0.0f, footOffset + radius, 0.0f);
            const f32 targetCy = hitPoint.y + (radius - hitNormal.x * (C.x - hitPoint.x) - hitNormal.z * (C.z - hitPoint.z)) / hitNormal.y;
            return targetCy - (footOffset + radius);
        }
        return hitPoint.y - footOffset;
    }

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

bool KinematicCharacterSystem::SweepCharacter(const Scene& scene, const CharacterControllerComponent& kcc,
    const vec3& fromPos, const vec3& direction, f32 distance, HitResult& outHit, entt::entity ignoreEntity)
{
    outHit.Reset();
    if (distance <= 1.0e-5f || !std::isfinite(distance)) return false;
    const auto& world = scene.GetRegistry();
    // 하향 스윕 시 바닥면과의 부동소수점 초기 겹침(enter=0)을 방지하기 위해 SkinWidth만큼 위에서 시작
    vec3 sweepOrigin = fromPos;
    f32 sweepDistance = distance;
    f32 startLift = 0.0f;
    if (direction.y < -0.1f)
    {
        startLift = kcc.SkinWidth;
        sweepOrigin.y += startLift;
        sweepDistance += startLift;
    }
    BoundingBox charBox = kcc.GetWorldAABB(sweepOrigin);
    if (std::abs(direction.y) < 0.1f)
    {
        // 수평 이동 시: 발바닥이 현재 딛고 있는 계단 상판과 마찰하지 않도록 하단을 살짝 띄움
        charBox.min.y += glm::min(kcc.SkinWidth + 0.02f, kcc.Height * 0.25f);
    }
    else
    {
        // 수직(상/하) 이동 시: 상향 이동 시에는 수평 폭을 SkinWidth만큼 축소하여 수직 벽 마찰을 방지하고,
        // 하향 이동 시에는 발바닥이 계단 모서리에 미리 떨어지지 않도록 최소 여유(0.01f)만 둠
        const f32 horizInset = (direction.y > 0.1f) ? glm::min(kcc.SkinWidth, kcc.Radius * 0.4f) : 0.01f;
        charBox.min.x += horizInset;
        charBox.max.x -= horizInset;
        charBox.min.z += horizInset;
        charBox.max.z -= horizInset;
        if (direction.y > 0.1f)
        {
            charBox.min.y += 0.01f;
        }
    }
    f32 closestDist = sweepDistance;
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
        const auto& worldTransform = world.get<WorldTransformComponent>(entity);
        const mat4 colliderWorld = worldTransform.worldMatrix * glm::translate(mat4(1.0f), collider.GetOffset());
        if (const auto* box = std::get_if<BoxShape>(&collider.GetShape()))
        {
            if (!box->HasValidParameters()) continue;
            const vec3 halfSize = box->Size * 0.5f;
            const vec3 worldCenter = vec3(colliderWorld[3]);
            const quat colRot = glm::quat_cast(colliderWorld);
            const bool isRotated = (std::abs(colRot.w * colRot.w - 1.0f) > 1.0e-3f);

            HitResult tempHit{};
            if (isRotated)
            {
                // 회전된 경사로 박스: 정확한 OBB(Oriented Bounding Box) 캡슐 스윕 검사
                Capsule charCap = kcc.GetWorldCapsule(sweepOrigin);
                // 수평/상향 이동 시 디딤 바닥과의 초기 겹침 방지 (바닥 구 살짝 리프트)
                // 수평/상향 이동이거나 지면 접선 방향 이동 시 디딤 바닥과의 초기 겹침 방지 (바닥 구 살짝 리프트)
                const bool isSurfaceMove = (direction.y >= -0.2f) || (kcc.GroundNormal.y > 0.0f && glm::dot(direction, kcc.GroundNormal) >= -0.1f);
                if (isSurfaceMove)
                {
                    const f32 bottomLift = glm::min(kcc.SkinWidth + 0.02f, kcc.Radius * 0.5f);
                    charCap.pointA.y += bottomLift;
                    if (charCap.pointA.y > charCap.pointB.y) charCap.pointA.y = charCap.pointB.y;
                }
                const BoundingBox localBox(-halfSize, halfSize);
                if (Collision::Sweep(charCap, direction, closestDist, localBox, colliderWorld, tempHit) &&
                    tempHit.hasHit && tempHit.distance < closestDist)
                {
                    if (tempHit.distance <= 1.0e-4f)
                    {
                        // 1) 현재 딛고 있는 지면 법선 기준으로 표면을 따라 이동하거나 멀어지는 경우 스킵
                        if (kcc.GroundNormal.y > 0.0f && glm::dot(direction, kcc.GroundNormal) >= -0.05f)
                        {
                            continue;
                        }
                        // 2) 충돌면 법선 기준으로 표면을 따라 이동하는 경우 스킵
                        if (tempHit.normal.y > 0.0f && glm::dot(direction, tempHit.normal) >= -0.05f)
                        {
                            continue;
                        }
                        const f32 cosAngle = glm::clamp(glm::dot(tempHit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
                        const f32 slope = glm::degrees(std::acos(cosAngle));
                        if (slope <= kcc.SlopeLimitDegrees && tempHit.normal.y > 0.0f)
                        {
                            continue;
                        }
                        const vec3 toHit = vec3(tempHit.point) - sweepOrigin;
                        if (glm::dot(toHit, direction) < -1.0e-3f)
                        {
                            continue;
                        }
                    }
                    closestDist = tempHit.distance;
                    outHit = tempHit;
                    hasHit = true;
                }
            }
            else
            {
                // 축 정렬(AABB) 박스: 고속 AABB 스윕 검사 (바닥 및 계단)
                const vec3 extent = glm::abs(vec3(colliderWorld[0])) * halfSize.x + glm::abs(vec3(colliderWorld[1])) * halfSize.y + glm::abs(vec3(colliderWorld[2])) * halfSize.z;
                const BoundingBox obstacle(worldCenter - extent, worldCenter + extent);

                if (Collision::Sweep(charBox, direction, closestDist, obstacle, tempHit) &&
                    tempHit.hasHit && tempHit.distance < closestDist)
                {
                    if (tempHit.distance <= 1.0e-4f)
                    {
                        if (kcc.GroundNormal.y > 0.0f && glm::dot(direction, kcc.GroundNormal) >= -0.05f)
                        {
                            continue;
                        }
                        if (tempHit.normal.y > 0.0f && glm::dot(direction, tempHit.normal) >= -0.05f)
                        {
                            continue;
                        }
                        const f32 cosAngle = glm::clamp(glm::dot(tempHit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
                        const f32 slope = glm::degrees(std::acos(cosAngle));
                        if (slope <= kcc.SlopeLimitDegrees && tempHit.normal.y > 0.0f)
                        {
                            continue;
                        }
                        const vec3 toHit = vec3(tempHit.point) - sweepOrigin;
                        if (glm::dot(toHit, direction) < -1.0e-3f)
                        {
                            continue;
                        }
                    }
                    closestDist = tempHit.distance;
                    outHit = tempHit;
                    hasHit = true;
                }
            }
        }
        else if (const auto* sphere = std::get_if<SphereShape>(&collider.GetShape()))
        {
            if (!sphere->HasValidParameters()) continue;
            const f32 maxScale = glm::max(glm::length(vec3(colliderWorld[0])), glm::max(glm::length(vec3(colliderWorld[1])), glm::length(vec3(colliderWorld[2]))));
            const vec3 worldCenter = vec3(colliderWorld[3]);
            const BoundingSphere obstacle(worldCenter, sphere->Radius * maxScale);
            HitResult tempHit{};
            const Capsule charCap = kcc.GetWorldCapsule(sweepOrigin);
            if (Collision::Sweep(charCap, direction, closestDist, obstacle, tempHit) &&
                tempHit.hasHit && tempHit.distance < closestDist)
            {
                if (tempHit.distance <= 1.0e-5f)
                {
                    const vec3 toHit = vec3(tempHit.point) - sweepOrigin;
                    if (glm::dot(toHit, direction) < -1.0e-3f)
                    {
                        continue;
                    }
                }
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
            const Capsule charCap = kcc.GetWorldCapsule(sweepOrigin);
            if (Collision::Sweep(charCap, direction, closestDist, obstacleCap, tempHit) &&
                tempHit.hasHit && tempHit.distance < closestDist)
            {
                if (tempHit.distance <= 1.0e-5f)
                {
                    const vec3 toHit = vec3(tempHit.point) - sweepOrigin;
                    if (glm::dot(toHit, direction) < -1.0e-3f)
                    {
                        continue;
                    }
                }
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
                    const vec3 bottomCenter = sweepOrigin + kcc.Center - vec3(0.0f, kcc.Height * 0.5f - kcc.Radius, 0.0f);
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
    if (hasHit && startLift > 0.0f)
    {
        outHit.distance = glm::max(0.0f, outHit.distance - startLift);
    }
    return hasHit;
}

bool KinematicCharacterSystem::DetectGround(const Scene& scene, const CharacterControllerComponent& kcc,
    const vec3& currentPos, HitResult& outGroundHit, entt::entity ignoreEntity, entt::entity* outHitEntity)
{
    outGroundHit.Reset();
    if (outHitEntity) *outHitEntity = entt::null;
    const bool wasOnSlopeOrGrounded = kcc.IsGrounded || (kcc.GroundAngle > 0.1f && kcc.GroundAngle < 89.0f);
    const f32 snapMargin = wasOnSlopeOrGrounded ? glm::max(kcc.StepOffset + 0.05f, 0.35f) : 0.02f;
    const f32 sweepDist = kcc.SkinWidth + snapMargin;
    // 1. 발바닥 전체 면적(부피) 하향 스윕
    HitResult sweepHit{};
    const bool hasSweepHit = SweepCharacter(scene, kcc, currentPos, vec3(0.0f, -1.0f, 0.0f), sweepDist, sweepHit, ignoreEntity);
    // 2. 중심 수직 레이캐스트 (MeshShape 바닥 감지 및 경사면 기하학 정밀 검출용)
    const vec3 bottomSphere = currentPos + kcc.Center - vec3(0.0f, kcc.Height * 0.5f - kcc.Radius, 0.0f);
    // 가파른 경사면(60도~75도)에서도 구 중심에서 수직 아래 경사면까지의 거리(R / cos(theta))를 확실히 커버하도록 rayDist 확장
    // 60도에서 R / cos(60) = 2.0 * R = 1.0m, 75도에서 R / cos(75) = 3.86 * R = 1.93m
    const f32 minSlopeRayDist = kcc.Radius * 3.5f;
    const f32 rayDist = glm::max(kcc.Radius + kcc.SkinWidth + snapMargin, minSlopeRayDist + snapMargin);
    Ray downRay(bottomSphere, vec3(0.0f, -1.0f, 0.0f));
    RaycastQuery query{};
    query.maxDistance = rayDist;
    query.layerMask = kcc.CollisionMask;
    query.includeTrigger = false;
    query.ignoreEntity = ignoreEntity;
    SceneRaycastHit raySceneHit{};
    const bool hasRayHit = PhysicsSystem::Raycast(scene, downRay, raySceneHit, query) && raySceneHit.Hit.hasHit;

    // 경사면(Slope > 5도) 감지 시: 하향 스윕의 법선 오염이나 접점 흔들림을 배제하고 기하학적으로 순수한 레이캐스트 우선 채택
    if (hasRayHit)
    {
        const f32 cosRay = glm::clamp(glm::dot(raySceneHit.Hit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
        const f32 raySlope = glm::degrees(std::acos(cosRay));
        if (raySlope > 5.0f && raySceneHit.Hit.normal.y > 0.1f)
        {
            outGroundHit = raySceneHit.Hit;
            if (outHitEntity) *outHitEntity = raySceneHit.Entity;
            return true;
        }
    }

    if (hasSweepHit && hasRayHit)
    {
        const f32 footY = currentPos.y + (kcc.Center.y - kcc.Height * 0.5f);
        vec3 horizVel = vec3(kcc.Velocity.x, 0.0f, kcc.Velocity.z);
        if (glm::length2(horizVel) <= 1.0e-4f && glm::length2(kcc.MoveInput) > 1.0e-4f)
        {
            horizVel = vec3(kcc.MoveInput.x, 0.0f, kcc.MoveInput.z);
        }
        bool preferSweep = false;
        if (glm::length2(horizVel) > 1.0e-4f)
        {
            const vec3 moveDir = glm::normalize(horizVel);
            const vec3 toSweep = vec3(sweepHit.point.x - currentPos.x, 0.0f, sweepHit.point.z - currentPos.z);
            const f32 dotVal = glm::dot(toSweep, moveDir);
            if (dotVal >= -1.0e-3f)
            {
                // 이동 방향 앞쪽의 지면: 더 높은 지면(계단 오르기) 우선 채택
                preferSweep = (sweepHit.point.y >= raySceneHit.Hit.point.y - 0.01f);
            }
            else
            {
                // 이동 방향 뒤쪽의 지면: 계단을 내려가는 중이므로 앞쪽(ray)의 낮은 지면 우선 채택
                preferSweep = (sweepHit.point.y < raySceneHit.Hit.point.y);
            }
        }
        else
        {
            // 정지 상태: 현재 발바닥 높이에 더 가까운 지면 우선
            preferSweep = std::abs(sweepHit.point.y - footY) <= std::abs(raySceneHit.Hit.point.y - footY);
        }
        if (preferSweep)
        {
            outGroundHit = sweepHit;
            if (outHitEntity && hasRayHit) *outHitEntity = raySceneHit.Entity;
        }
        else
        {
            outGroundHit = raySceneHit.Hit;
            if (outHitEntity) *outHitEntity = raySceneHit.Entity;
        }
        return true;
    }
    if (hasSweepHit)
    {
        outGroundHit = sweepHit;
        return true;
    }
    if (hasRayHit)
    {
        outGroundHit = raySceneHit.Hit;
        if (outHitEntity) *outHitEntity = raySceneHit.Entity;
        return true;
    }
    return false;
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

            // 위치 이동 상속 (엔티티 전환 시 1.2m 튕김 방지: 단일 프레임 0.5m 이하의 실제 플랫폼 이동만 반영)
            const vec3 deltaPlatformPos = groundPos - kcc.LastGroundPos;
            if (glm::length2(deltaPlatformPos) < 0.25f)
            {
                currentPos += deltaPlatformPos;
            }

            // 플랫폼 중심 기준 회전 상속
            const quat deltaPlatformRot = groundRot * glm::inverse(kcc.LastGroundRot);
            if (glm::length2(deltaPlatformRot - quat(1.0f, 0.0f, 0.0f, 0.0f)) > 1.0e-6f && glm::length2(deltaPlatformPos) < 0.25f)
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

    // 1. 이미 착지 상태이거나 지면에 닿아있을 때 지면 상태 검사
    HitResult groundHit{};
    entt::entity hitEntity = entt::null;
    const bool foundGround = DetectGround(scene, kcc, currentPos, groundHit, entity, &hitEntity);
    bool onSteepSlope = false;
    vec3 steepSlopeNormal = vec3(0.0f, 1.0f, 0.0f);

    // 점프하여 위로 솟구치는 중(Velocity.y > 0.05f)에는 지면으로 재흡착하지 않고 점프 상승 궤적을 온전히 유지
    if (kcc.Velocity.y > 0.05f)
    {
        kcc.IsGrounded = false;
        kcc.GroundEntity = entt::null;
    }
    else if (foundGround && groundHit.normal.y > 0.0f)
    {
        const f32 cosAngle = glm::clamp(glm::dot(groundHit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
        const f32 slopeAngle = glm::degrees(std::acos(cosAngle));
        if (slopeAngle <= kcc.SlopeLimitDegrees)
        {
            if (kcc.IsGrounded)
            {
                currentPos.y = CalculateGroundContactY(currentPos, footOffset, kcc.Radius, groundHit.point, groundHit.normal);
                kcc.Velocity.y = 0.0f;
            }
            kcc.IsGrounded = true;
            kcc.GroundNormal = groundHit.normal;
            kcc.GroundAngle = slopeAngle;
            if (hitEntity != entt::null && kcc.GroundEntity != hitEntity)
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
            // 급경사면(SlopeLimitDegrees 초과): 걸을 수 없고 미끄러져야 하는 상태
            onSteepSlope = true;
            steepSlopeNormal = groundHit.normal;
            kcc.IsGrounded = false;
            kcc.GroundEntity = entt::null;
            kcc.GroundNormal = groundHit.normal;
            kcc.GroundAngle = slopeAngle;
            currentPos.y = CalculateGroundContactY(currentPos, footOffset, kcc.Radius, groundHit.point, groundHit.normal);
        }
    }
    else
    {
        kcc.IsGrounded = false;
        kcc.GroundEntity = entt::null;
    }

    // 2. 점프 처리 (급경사면에서는 점프 불가)
    bool justJumped = false;
    if (kcc.JumpRequested)
    {
        if (kcc.IsGrounded && !onSteepSlope)
        {
            kcc.Velocity.y = kcc.JumpForce;
            kcc.IsGrounded = false;
            kcc.GroundEntity = entt::null;
            justJumped = true;
        }
        kcc.JumpRequested = false;
    }

    // 3. 중력 가속도 및 급경사면 미끄러짐(Slope Sliding) 적용
    if (onSteepSlope)
    {
        // 경사면 안으로 파고드는 불필요한 속도 성분 제거 (경사면 접선 평면 정렬)
        const f32 intoSlope = glm::dot(kcc.Velocity, steepSlopeNormal);
        if (intoSlope < 0.0f)
        {
            kcc.Velocity -= steepSlopeNormal * intoSlope;
        }

        const vec3 grav = vec3(0.0f, kcc.Gravity, 0.0f);
        const vec3 slideAccel = grav - steepSlopeNormal * glm::dot(grav, steepSlopeNormal);
        kcc.Velocity += slideAccel * dt;

        const f32 maxSlideSpeed = kcc.RunSpeed * 1.5f;
        const f32 currentSlideSpeed = glm::length(kcc.Velocity);
        if (currentSlideSpeed > maxSlideSpeed)
        {
            kcc.Velocity = (kcc.Velocity / currentSlideSpeed) * maxSlideSpeed;
        }
    }
    else if (!kcc.IsGrounded)
    {
        kcc.Velocity.y += kcc.Gravity * dt;
    }

    // 4. 이동 목표 속도 산출 (경사면 접선 방향 투영 적용)
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
    vec3 desiredHorizVel = inputDir * targetSpeed;

    if (onSteepSlope)
    {
        // 급경사면에서는 위로 거슬러 올라가는(Uphill) 입력을 차단
        const vec3 downhill = glm::normalize(vec3(steepSlopeNormal.x, 0.0f, steepSlopeNormal.z));
        const f32 uphillDot = glm::dot(desiredHorizVel, downhill);
        if (uphillDot < 0.0f)
        {
            desiredHorizVel -= downhill * uphillDot;
        }
        kcc.Velocity.x = glm::mix(kcc.Velocity.x, desiredHorizVel.x + kcc.Velocity.x, glm::clamp(5.0f * dt, 0.0f, 1.0f));
        kcc.Velocity.z = glm::mix(kcc.Velocity.z, desiredHorizVel.z + kcc.Velocity.z, glm::clamp(5.0f * dt, 0.0f, 1.0f));
    }
    else if (kcc.IsGrounded)
    {
        // 완만한 경사면 위에서 이동 시 지면 법선에 속도를 투영하여 경사면을 따라 대각선으로 부드럽게 이동
        if (kcc.GroundAngle > 0.5f && kcc.GroundNormal.y > 0.001f && glm::length2(desiredHorizVel) > 1.0e-4f)
        {
            vec3 slopeDir = desiredHorizVel - kcc.GroundNormal * glm::dot(desiredHorizVel, kcc.GroundNormal);
            if (glm::length2(slopeDir) > 1.0e-4f)
            {
                kcc.Velocity = glm::normalize(slopeDir) * targetSpeed;
            }
            else
            {
                kcc.Velocity = desiredHorizVel;
            }
        }
        else
        {
            // 급경사면이나 질주에서 평지로 전환될 때 단번에 속도를 끊지 않고 관성을 부드럽게 감속 보간
            const f32 decelRate = 12.0f * dt;
            kcc.Velocity.x = glm::mix(kcc.Velocity.x, desiredHorizVel.x, glm::clamp(decelRate, 0.0f, 1.0f));
            kcc.Velocity.y = 0.0f;
            kcc.Velocity.z = glm::mix(kcc.Velocity.z, desiredHorizVel.z, glm::clamp(decelRate, 0.0f, 1.0f));
        }
    }
    else
    {
        // 체공 중 AirControl 보간
        kcc.Velocity.x = glm::mix(kcc.Velocity.x, desiredHorizVel.x, glm::clamp(kcc.AirControl * 10.0f * dt, 0.0f, 1.0f));
        kcc.Velocity.z = glm::mix(kcc.Velocity.z, desiredHorizVel.z, glm::clamp(kcc.AirControl * 10.0f * dt, 0.0f, 1.0f));
    }

    // 5. 이동 및 장애물 슬라이딩 (Slide & Step-up / Step-down / Crevice Sliding)
    vec3 moveDelta = (kcc.IsGrounded || onSteepSlope) ? (kcc.Velocity * dt) : (vec3(kcc.Velocity.x, 0.0f, kcc.Velocity.z) * dt);
    f32 moveDist = glm::length(moveDelta);

    if (moveDist > 1.0e-5f)
    {
        vec3 moveDir = moveDelta / moveDist;
        f32 remainingDist = moveDist;
        const int maxBounces = 3;
        bool stepped = false;
        vec3 prevWallNormal(0.0f);
        bool hasPrevWall = false;

        for (int bounce = 0; bounce < maxBounces && remainingDist > 1.0e-5f; ++bounce)
        {
            HitResult hit{};
            if (SweepCharacter(scene, kcc, currentPos, moveDir, remainingDist, hit, entity))
            {
                const f32 travelDist = glm::max(0.0f, hit.distance - kcc.SkinWidth);
                currentPos += moveDir * travelDist;
                remainingDist -= travelDist;

                const f32 hitSlopeCos = glm::clamp(glm::dot(hit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
                const f32 hitSlopeAngle = glm::degrees(std::acos(hitSlopeCos));
                const bool isWalkableSlope = (hitSlopeAngle <= kcc.SlopeLimitDegrees && hit.normal.y > 0.0f);

                // 계단/턱 오르기 (Step-Up: 상향 헤드룸 천장 검사 포함)
                if (kcc.IsGrounded && kcc.StepOffset > 0.001f)
                {
                    HitResult ceilHit{};
                    f32 allowedStepUp = kcc.StepOffset;
                    if (SweepCharacter(scene, kcc, currentPos, vec3(0.0f, 1.0f, 0.0f), kcc.StepOffset, ceilHit, entity))
                    {
                        allowedStepUp = glm::max(0.0f, ceilHit.distance - kcc.SkinWidth);
                    }
                    if (allowedStepUp > 0.05f)
                    {
                        const vec3 stepUpPos = currentPos + vec3(0.0f, allowedStepUp, 0.0f);
                        HitResult forwardHit{};
                        const f32 minStepForward = glm::max(kcc.SkinWidth * 3.0f, 0.15f);
                        const f32 forwardDist = glm::max(remainingDist, minStepForward);
                        f32 actualForward = forwardDist;
                        if (SweepCharacter(scene, kcc, stepUpPos, moveDir, forwardDist, forwardHit, entity))
                        {
                            actualForward = glm::max(0.0f, forwardHit.distance - kcc.SkinWidth);
                        }
                        if (actualForward > 0.01f)
                        {
                            const vec3 forwardPos = stepUpPos + moveDir * actualForward;
                            HitResult groundHit{};
                            const f32 downCheckDist = allowedStepUp + kcc.SkinWidth + 0.05f;
                            if (SweepCharacter(scene, kcc, forwardPos, vec3(0.0f, -1.0f, 0.0f), downCheckDist, groundHit, entity))
                            {
                                const f32 cosAngle = glm::clamp(glm::dot(groundHit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
                                const f32 slope = glm::degrees(std::acos(cosAngle));
                                const f32 landedY = CalculateGroundContactY(forwardPos, footOffset, kcc.Radius, groundHit.point, groundHit.normal);
                                if (slope <= kcc.SlopeLimitDegrees && landedY > currentPos.y + 0.005f)
                                {
                                    currentPos = forwardPos;
                                    currentPos.y = landedY;
                                    kcc.GroundNormal = groundHit.normal;
                                    kcc.GroundAngle = slope;
                                    kcc.IsGrounded = true;
                                    kcc.Velocity.y = 0.0f;
                                    stepped = true;
                                    remainingDist = 0.0f;
                                }
                            }
                        }
                    }
                }
                if (!stepped)
                {
                    // 침투 해소(Depenetration Push): 틈새에 너무 좁게 밀착된 경우 밀어내어 탈출 지원 (수직 벽/장애물 대상)
                    if (hit.distance < kcc.SkinWidth && !isWalkableSlope && hit.normal.y <= 0.2f)
                    {
                        const f32 pushOut = (kcc.SkinWidth - hit.distance) * 0.5f;
                        currentPos += hit.normal * pushOut;
                    }

                    vec3 currentWallNormal = hit.normal;
                    if (hit.normal.y <= 0.0f)
                    {
                        vec3 horizNormal = vec3(hit.normal.x, 0.0f, hit.normal.z);
                        if (glm::length2(horizNormal) > 1.0e-4f)
                        {
                            currentWallNormal = glm::normalize(horizNormal);
                        }
                    }

                    // 다중 벽 충돌: 이전 벽과 이번 벽이 서로 마주보는 틈새(Crevice / Wedge)인지 검사
                    if (hasPrevWall && glm::dot(prevWallNormal, currentWallNormal) < -0.01f)
                    {
                        // 틈새 골짜기 선(교선) 방향으로 이동 벡터 정렬
                        vec3 creviceDir = glm::cross(prevWallNormal, currentWallNormal);
                        if (glm::length2(creviceDir) > 1.0e-4f)
                        {
                            creviceDir = glm::normalize(creviceDir);
                            if (glm::dot(moveDir, creviceDir) < 0.0f) creviceDir = -creviceDir;
                            moveDir = creviceDir;
                        }
                        else
                        {
                            // 완전 대향 평행 벽인 경우: 벽 법선에 수직인 수평 통로 방향으로 설정
                            vec3 hallwayDir = glm::normalize(glm::cross(currentWallNormal, vec3(0.0f, 1.0f, 0.0f)));
                            if (glm::dot(moveDir, hallwayDir) < 0.0f) hallwayDir = -hallwayDir;
                            moveDir = hallwayDir;
                        }
                    }
                    else
                    {
                        // 단일 벽 일반 슬라이딩 투영
                        if (glm::dot(moveDir, currentWallNormal) < -1.0e-4f)
                        {
                            moveDir = moveDir - currentWallNormal * glm::dot(moveDir, currentWallNormal);
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

                    prevWallNormal = currentWallNormal;
                    hasPrevWall = true;
                }
            }
            else
            {
                currentPos += moveDir * remainingDist;
                remainingDist = 0.0f;
            }
        }
        // 계단 내려가기 (Step-Down Ground Snapping: 점프 상승 중에는 실행 방지)
        if (kcc.IsGrounded && !justJumped && !stepped && kcc.Velocity.y <= 0.05f && kcc.StepOffset > 0.001f)
        {
            HitResult stepDownHit{};
            entt::entity stepDownEntity = entt::null;
            if (DetectGround(scene, kcc, currentPos, stepDownHit, entity, &stepDownEntity))
            {
                const f32 cosAngle = glm::clamp(glm::dot(stepDownHit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
                const f32 slope = glm::degrees(std::acos(cosAngle));
                if (slope <= kcc.SlopeLimitDegrees)
                {
                    currentPos.y = CalculateGroundContactY(currentPos, footOffset, kcc.Radius, stepDownHit.point, stepDownHit.normal);
                    kcc.GroundNormal = stepDownHit.normal;
                    kcc.GroundAngle = slope;
                    kcc.IsGrounded = true;
                    if (stepDownEntity != entt::null)
                    {
                        kcc.GroundEntity = stepDownEntity;
                        if (const auto* gt = world.try_get<TransformComponent>(stepDownEntity))
                        {
                            kcc.LastGroundPos = gt->position;
                            kcc.LastGroundRot = gt->rotation;
                        }
                    }
                }
            }
        }
    }

    // 6. 수직 이동 및 천장/바닥 충돌 (급경사면 미끄러짐 중에는 이미 표면 밀착 처리됨)
    if (!kcc.IsGrounded && !onSteepSlope)
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
                const f32 cosAngle = glm::clamp(glm::dot(fallHit.normal, vec3(0.0f, 1.0f, 0.0f)), -1.0f, 1.0f);
                const f32 slopeAngle = glm::degrees(std::acos(cosAngle));
                if (slopeAngle <= kcc.SlopeLimitDegrees)
                {
                    currentPos.y = CalculateGroundContactY(currentPos, footOffset, kcc.Radius, fallHit.point, fallHit.normal);
                    kcc.Velocity.y = 0.0f;
                    kcc.IsGrounded = true;
                    kcc.GroundNormal = fallHit.normal;
                    kcc.GroundAngle = slopeAngle;
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
                    currentPos.y = CalculateGroundContactY(currentPos, footOffset, kcc.Radius, fallHit.point, fallHit.normal);
                    kcc.IsGrounded = false;
                    kcc.GroundNormal = fallHit.normal;
                    kcc.GroundAngle = slopeAngle;
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
    (void)activeScenes;
    (void)dt;
}

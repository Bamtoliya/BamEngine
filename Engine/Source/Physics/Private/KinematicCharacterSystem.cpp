#include "KinematicCharacterSystem.h"
#include "ComponentEnable.h"
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
#include "AnimationSceneQuery.h"

namespace
{
    constexpr f32 GroundContactTolerance = 1.0e-4f;
    constexpr f32 SlopeNormalTolerance = 1.0e-5f;
    constexpr f32 SoftLandingMinPrevSlopeDegrees = 5.0f;
    constexpr f32 SoftLandingAngleDeltaDegrees = 2.0f;
    constexpr f32 SoftLandingMaxSpeed = 3.5f;
    constexpr f32 SoftLandingMinStep = 0.005f;

    bool IsWalkableNormal(const CharacterControllerComponent& kcc, f32 normalY)
    {
        const f32 minNormalY = glm::cos(glm::radians(kcc.SlopeLimitDegrees));
        return normalY > 0.0f && normalY + SlopeNormalTolerance >= minNormalY;
    }

    f32 GroundSnapDistance(const CharacterControllerComponent& kcc)
    {
        return kcc.SkinWidth + glm::max(kcc.StepOffset + 0.05f, 0.35f);
    }

    bool IsWorldAxisAligned(const mat4& matrix)
    {
        for (int i = 0; i < 3; ++i)
        {
            vec3 offAxis(matrix[i]);
            const f32 scale = std::abs(offAxis[i]);
            offAxis[i] = 0.0f;
            if (glm::length2(offAxis) > scale * scale * 1.0e-10f) return false;
        }
        return true;
    }

}

struct KinematicCharacterSystem::GroundSupport
{
    SceneRaycastHit Contact{};
    f32 Drop = 0.0f;
};

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
            auto& kcc = view.get<CharacterControllerComponent>(entity);
            if (!IsComponentEnabled<CharacterControllerComponent>(world, entity))
            {
                kcc.MoveInput = vec3(0.0f);
                kcc.JumpRequested = false;
                continue;
            }
            const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
            if (hierarchy && hierarchy->parent != entt::null) continue;
            const auto* flag = world.try_get<FlagComponent>(entity);
            if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) || HasFlag(flag->flags, EEntityFlag::Dead))) continue;
            UpdateCharacter(*scene, entity, kcc, dt);
        }
    }
}

bool KinematicCharacterSystem::SweepCharacter(const Scene& scene, const CharacterControllerComponent& kcc,
    const vec3& fromPos, const vec3& direction, f32 distance, HitResult& outHit, entt::entity ignoreEntity)
{
    SceneRaycastHit contact{};
    const bool found = SweepCharacterContact(scene, kcc, fromPos, direction, distance, contact, ignoreEntity, SweepPurpose::Movement);
    outHit = contact.Hit;
    return found;
}

bool KinematicCharacterSystem::SweepCharacterContact(const Scene& scene, const CharacterControllerComponent& kcc,
    const vec3& fromPos, const vec3& direction, f32 distance, SceneRaycastHit& outContact,
    entt::entity ignoreEntity, SweepPurpose purpose)
{
    outContact = {};
    outContact.Hit.Reset();
    if (distance <= 1.0e-5f || !std::isfinite(distance)) return false;
    const auto& world = scene.GetRegistry();
    // 지지면 검사에서만 위에서 시작해 얕게 겹친 지면의 보정 거리도 구한다.
    vec3 sweepOrigin = fromPos;
    f32 sweepDistance = distance;
    f32 startLift = 0.0f;
    if (purpose == SweepPurpose::GroundSupport)
    {
        startLift = kcc.SkinWidth;
        sweepOrigin.y += startLift;
        sweepDistance += startLift;
    }
    BoundingBox charBox = kcc.GetWorldAABB(sweepOrigin);
    if (std::abs(direction.y) < 0.1f)
    {
        // 수평 이동 중 바닥과의 접촉 오차만 허용한다.
        charBox.min.y += GroundContactTolerance;
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
    const auto recordHit = [&](entt::entity entity, const HitResult& hit)
        {
            if (purpose == SweepPurpose::GroundSupport && hit.normal.y <= GroundContactTolerance)
                return;
            closestDist = hit.distance;
            outContact = { entity, hit };
            hasHit = true;
        };
    auto view = world.view<ColliderComponent, WorldTransformComponent>();
    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
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

            HitResult tempHit{};
            if (!IsWorldAxisAligned(colliderWorld))
            {
                const Capsule charCap = kcc.GetWorldCapsule(sweepOrigin);
                const BoundingBox localBox(-halfSize, halfSize);
                if (Collision::Sweep(charCap, direction, closestDist, localBox, colliderWorld, tempHit) &&
                    tempHit.hasHit && tempHit.distance < closestDist)
                {
                    if (tempHit.distance <= GroundContactTolerance &&
                        glm::dot(direction, tempHit.normal) >= -GroundContactTolerance)
                        continue;
                    recordHit(entity, tempHit);
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
                    if (tempHit.distance <= GroundContactTolerance)
                    {
                        HitResult overlap{};
                        if (Collision::Intersects(kcc.GetWorldCapsule(sweepOrigin), obstacle, overlap))
                            tempHit.normal = overlap.normal;
                        if (glm::dot(direction, tempHit.normal) >= -GroundContactTolerance)
                            continue;
                    }
                    recordHit(entity, tempHit);
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
                recordHit(entity, tempHit);
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
                recordHit(entity, tempHit);
            }
        }
        else if (const auto* meshShape = purpose == SweepPurpose::Movement ? std::get_if<MeshShape>(&collider.GetShape()) : nullptr)
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
                        recordHit(entity, tempHit);
                    }
                }
            }
        }
    }
    if (hasHit && startLift > 0.0f)
    {
        outContact.Hit.distance -= startLift;
        if (purpose == SweepPurpose::Movement)
            outContact.Hit.distance = glm::max(0.0f, outContact.Hit.distance);
    }
    return hasHit;
}

bool KinematicCharacterSystem::DetectGround(const Scene& scene, const CharacterControllerComponent& kcc,
    const vec3& currentPos, HitResult& outGroundHit, entt::entity ignoreEntity, entt::entity* outHitEntity)
{
    const f32 maxDrop = kcc.IsGrounded ? GroundSnapDistance(kcc) : 0.0f;
    GroundSupport ground{};
    const bool found = FindGroundSupport(scene, kcc, currentPos, maxDrop, ground, ignoreEntity);
    if (found)
        outGroundHit = ground.Contact.Hit;
    else
        outGroundHit.Reset();
    if (outHitEntity) *outHitEntity = ground.Contact.Entity;
    return found;
}

bool KinematicCharacterSystem::FindGroundSupport(const Scene& scene, const CharacterControllerComponent& kcc,
    const vec3& currentPos, f32 maxDrop, GroundSupport& outGround, entt::entity ignoreEntity)
{
    outGround = {};
    const vec3 down(0.0f, -1.0f, 0.0f);
    GroundSupport swept{};
    const bool hasSweepHit = SweepCharacterContact(scene, kcc, currentPos, down, maxDrop + GroundContactTolerance,
        swept.Contact, ignoreEntity, SweepPurpose::GroundSupport) && swept.Contact.Hit.normal.y > GroundContactTolerance;
    swept.Drop = swept.Contact.Hit.distance;

    // 경사면 검출용 레이 거리는 캡슐이 실제로 닿을 수 있는 낙하 거리와 별도로 계산한다.
    const Capsule capsule = kcc.GetWorldCapsule(currentPos);
    RaycastQuery query{};
    query.maxDistance = glm::max(capsule.radius + kcc.SkinWidth, capsule.radius * 3.5f) + maxDrop + GroundContactTolerance;
    query.layerMask = kcc.CollisionMask;
    query.includeTrigger = false;
    query.ignoreEntity = ignoreEntity;
    GroundSupport ray{};
    const auto acceptRaySupport = [&](const SceneRaycastHit& candidate)
        {
            const auto& hit = candidate.Hit;
            if (hit.normal.y <= GroundContactTolerance)
                return false;
            const f32 separation = glm::dot(hit.normal, capsule.pointA - hit.point) - capsule.radius;
            const f32 drop = separation / hit.normal.y;
            if (separation < -kcc.SkinWidth - GroundContactTolerance || drop > maxDrop + GroundContactTolerance)
                return false;
            ray = { candidate, drop };
            return true;
        };
    const Ray downRay(capsule.pointA, down);
    SceneRaycastHit firstRayHit{};
    bool hasRayHit = false;
    if (PhysicsSystem::Raycast(scene, downRay, firstRayHit, query))
    {
        hasRayHit = acceptRaySupport(firstRayHit);
        // 가까운 아랫면이나 내부 히트로 지지면이 가려진 경우에는 더 아래의 유효 접촉을 찾는다.
        if (!hasRayHit && firstRayHit.Hit.distance <= capsule.radius + maxDrop + kcc.SkinWidth)
        {
            for (const auto& candidate : PhysicsSystem::RaycastAll(scene, downRay, query))
            {
                if (!acceptRaySupport(candidate)) continue;
                hasRayHit = true;
                break;
            }
        }
    }

    if (!hasSweepHit && !hasRayHit) return false;
    if (!hasSweepHit || !hasRayHit)
    {
        outGround = hasSweepHit ? swept : ray;
        return true;
    }

    const f32 rayNormalY = ray.Contact.Hit.normal.y;
    const bool refineSlope = swept.Contact.Entity == ray.Contact.Entity && std::abs(swept.Drop - ray.Drop) <= GroundContactTolerance &&
        rayNormalY > 0.1f && rayNormalY < glm::cos(glm::radians(5.0f));
    // 발 중심 레이가 낮은 단에 닿아도 캡슐 발밑에 더 높은 지지면이 남아 있으면 그 면을 유지한다.
    outGround = ray.Drop < swept.Drop || refineSlope ? ray : swept;
    return true;
}

void KinematicCharacterSystem::UpdateCharacter(Scene& scene, entt::entity entity, CharacterControllerComponent& kcc, f32 dt)
{
    auto& world = scene.GetRegistry();
    auto& transform = world.get<TransformComponent>(entity);
    vec3 currentPos = transform.position;
    const auto setGroundEntity = [&](entt::entity ground)
        {
            if (kcc.GroundEntity == ground) return;
            kcc.GroundEntity = ground;
            if (ground == entt::null) return;
            if (const auto* groundTransform = world.try_get<TransformComponent>(ground))
            {
                kcc.LastGroundPos = groundTransform->position;
                kcc.LastGroundRot = groundTransform->rotation;
            }
        };
    const auto setGroundContact = [&](const SceneRaycastHit& contact)
        {
            const bool walkable = IsWalkableNormal(kcc, contact.Hit.normal.y);
            if (walkable && !kcc.IsGrounded)
                kcc.GroundEntity = entt::null;
            kcc.IsGrounded = walkable;
            kcc.GroundNormal = contact.Hit.normal;
            kcc.GroundAngle = glm::degrees(std::acos(glm::clamp(kcc.GroundNormal.y, -1.0f, 1.0f)));
            setGroundEntity(contact.Entity);
        };
    f32 remainingSoftDrop = glm::max(SoftLandingMinStep, SoftLandingMaxSpeed * dt);
    const auto applyGroundSnap = [&](const GroundSupport& support)
        {
            const vec3 prevNormal = kcc.GroundNormal;
            const f32 prevAngle = kcc.GroundAngle;
            const bool targetWalkable = IsWalkableNormal(kcc, support.Contact.Hit.normal.y);
            const f32 targetAngle = glm::degrees(std::acos(glm::clamp(support.Contact.Hit.normal.y, -1.0f, 1.0f)));
            const bool shouldSoftLand = targetWalkable && support.Drop > 0.0f &&
                prevAngle >= SoftLandingMinPrevSlopeDegrees &&
                targetAngle + SoftLandingAngleDeltaDegrees < prevAngle;

            if (shouldSoftLand && support.Drop > remainingSoftDrop)
            {
                const f32 appliedDrop = remainingSoftDrop;
                const f32 blend = glm::clamp(appliedDrop / support.Drop, 0.0f, 1.0f);
                currentPos.y -= appliedDrop;
                remainingSoftDrop = 0.0f;
                setGroundContact(support.Contact);
                kcc.GroundNormal = glm::normalize(glm::mix(prevNormal, support.Contact.Hit.normal, blend));
                kcc.GroundAngle = glm::degrees(std::acos(glm::clamp(kcc.GroundNormal.y, -1.0f, 1.0f)));
                return;
            }

            if (shouldSoftLand && support.Drop > 0.0f)
            {
                remainingSoftDrop = glm::max(0.0f, remainingSoftDrop - support.Drop);
            }
            currentPos.y -= support.Drop;
            setGroundContact(support.Contact);
        };
    if (world.valid(kcc.GroundEntity))
    {
        const auto* collider = TryGetEnabledComponent<ColliderComponent>(world, kcc.GroundEntity);
        const bool supportsCharacter = (collider && collider->GetSettings().Enabled && !collider->GetSettings().IsTrigger) ||
            TryGetEnabledComponent<TerrainComponent>(world, kcc.GroundEntity);
        if (!supportsCharacter) kcc.GroundEntity = entt::null;
    }
    const bool wasSliding = !kcc.IsGrounded && world.valid(kcc.GroundEntity) &&
        !IsWalkableNormal(kcc, kcc.GroundNormal.y);

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
    else if (!wasSliding)
    {
        kcc.GroundEntity = entt::null;
    }

    // 1. 기존 지지면을 유지하거나 현재 위치에서 닿은 면을 확인한다. 공중 낙하의 착지는 수직 이동에서 처리한다.
    GroundSupport ground{};
    const bool hadSupport = kcc.IsGrounded || wasSliding;
    // 오르막 접선 속도의 Y 성분과 지지면에서 멀어지는 속도를 구분한다.
    const bool checkExistingContact = hadSupport ? glm::dot(kcc.Velocity, kcc.GroundNormal) <= 0.05f : kcc.Velocity.y == 0.0f;
    const f32 maxSupportDrop = kcc.IsGrounded ? GroundSnapDistance(kcc) : (wasSliding ? kcc.SkinWidth : 0.0f);
    const bool foundGround = checkExistingContact && FindGroundSupport(scene, kcc, currentPos, maxSupportDrop, ground, entity) &&
        (!wasSliding || ground.Contact.Entity == kcc.GroundEntity || ground.Drop <= GroundContactTolerance);
    bool onSteepSlope = false;
    vec3 steepSlopeNormal = vec3(0.0f, 1.0f, 0.0f);

    if (foundGround)
    {
        applyGroundSnap(ground);
        if (kcc.IsGrounded)
        {
            kcc.Velocity.y = 0.0f;
        }
        else
        {
            // 급경사면(SlopeLimitDegrees 초과): 걸을 수 없고 미끄러져야 하는 상태
            onSteepSlope = true;
            steepSlopeNormal = kcc.GroundNormal;
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

    // 3. 공중 중력. 급경사에서는 입력을 합친 뒤 중력까지 접선에 투영한다.
    if (!kcc.IsGrounded && !onSteepSlope)
    {
        kcc.Velocity.y += kcc.Gravity * dt;
    }

    // 4. 이동 목표 속도 산출 (경사면 접선 방향 투영 적용)
    const vec3 horizontalInput(kcc.MoveInput.x, 0.0f, kcc.MoveInput.z);
    const bool hasMoveInput = glm::length2(horizontalInput) > 1.0e-4f;
    const vec3 inputDir = hasMoveInput ? glm::normalize(horizontalInput) : vec3(0.0f);

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
        const vec3 gravity(0.0f, kcc.Gravity, 0.0f);
        kcc.Velocity += gravity * dt + desiredHorizVel * glm::clamp(5.0f * dt, 0.0f, 1.0f);
        kcc.Velocity -= steepSlopeNormal * glm::dot(kcc.Velocity, steepSlopeNormal);

        // 진입할 때 남은 속도로 등판 한계를 넘지 않도록 오르막 성분을 제거한다.
        const vec3 downhillTangent = vec3(0.0f, -1.0f, 0.0f) + steepSlopeNormal * steepSlopeNormal.y;
        const f32 alongDownhill = glm::dot(kcc.Velocity, downhillTangent);
        if (alongDownhill < 0.0f)
            kcc.Velocity -= downhillTangent * (alongDownhill / glm::length2(downhillTangent));

        const f32 maxSlideSpeed = kcc.RunSpeed * 1.5f;
        const f32 slideSpeed = glm::length(kcc.Velocity);
        if (slideSpeed > maxSlideSpeed && slideSpeed > 0.0f)
            kcc.Velocity *= maxSlideSpeed / slideSpeed;
    }
    else if (kcc.IsGrounded)
    {
        // 완만한 경사면 위에서 이동 시 지면 법선에 속도를 투영하여 경사면을 따라 대각선으로 부드럽게 이동
        if (kcc.GroundAngle > 0.5f && kcc.GroundNormal.y > 0.001f)
        {
            const vec3 slopeDir = desiredHorizVel - kcc.GroundNormal * glm::dot(desiredHorizVel, kcc.GroundNormal);
            if (glm::length2(slopeDir) > 1.0e-4f)
            {
                kcc.Velocity = glm::normalize(slopeDir) * targetSpeed;
            }
            else
            {
                kcc.Velocity = vec3(0.0f);
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
    const bool movingOnSurface = kcc.IsGrounded || onSteepSlope;
    const vec3 moveVelocity = movingOnSurface ? kcc.Velocity : vec3(kcc.Velocity.x, 0.0f, kcc.Velocity.z);
    const vec3 moveDelta = moveVelocity * dt;
    f32 moveDist = glm::length(moveDelta);

    if (moveDist > 1.0e-5f)
    {
        const f32 surfaceStartY = currentPos.y;
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

                const bool isWalkableSlope = IsWalkableNormal(kcc, hit.normal.y);

                // 계단/턱 오르기 (Step-Up: 상향 헤드룸 천장 검사 포함)
                if (kcc.IsGrounded && hasMoveInput && kcc.StepOffset > 0.001f)
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
                            GroundSupport stepUp{};
                            const f32 downCheckDist = allowedStepUp + kcc.SkinWidth + 0.05f;
                            if (FindGroundSupport(scene, kcc, forwardPos, downCheckDist, stepUp, entity))
                            {
                                const auto& groundHit = stepUp.Contact.Hit;
                                const f32 landedY = forwardPos.y - stepUp.Drop;
                                const f32 rise = landedY - currentPos.y;
                                if (IsWalkableNormal(kcc, groundHit.normal.y) && rise > 0.005f &&
                                    rise <= allowedStepUp + GroundContactTolerance)
                                {
                                    currentPos = forwardPos;
                                    currentPos.y = landedY;
                                    setGroundContact(stepUp.Contact);
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
                    const vec3 horizontalNormal(hit.normal.x, 0.0f, hit.normal.z);
                    const bool hasHorizontalNormal = glm::length2(horizontalNormal) > 1.0e-4f;
                    if (hit.distance < kcc.SkinWidth && !isWalkableSlope && hit.normal.y <= 0.2f && hasHorizontalNormal)
                    {
                        const f32 pushOut = (kcc.SkinWidth - hit.distance) * 0.5f;
                        currentPos += glm::normalize(horizontalNormal) * pushOut;
                    }

                    vec3 currentWallNormal = hit.normal;
                    if (!movingOnSurface || hit.normal.y <= 0.0f)
                    {
                        if (hasHorizontalNormal)
                            currentWallNormal = glm::normalize(horizontalNormal);
                        else if (!movingOnSurface)
                            break;
                    }

                    // 다중 벽 충돌: 이전 벽과 이번 벽이 서로 마주보는 틈새(Crevice / Wedge)인지 검사
                    if (hasPrevWall && glm::dot(prevWallNormal, currentWallNormal) < -0.01f)
                    {
                        // 틈새 골짜기 선(교선) 방향으로 이동 벡터 정렬
                        vec3 creviceDir = glm::cross(prevWallNormal, currentWallNormal);
                        if (movingOnSurface && glm::length2(creviceDir) > 1.0e-4f)
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

        // 경사를 따라 실제로 내려간 범위 안의 접촉을 확인한다. 이동 후 높이에서만 검사하면 이미 지나친 바닥을 놓친다.
        bool reachedSupport = false;
        if (movingOnSurface && !stepped && currentPos.y < surfaceStartY)
        {
            const vec3 descentStart(currentPos.x, surfaceStartY, currentPos.z);
            GroundSupport crossing{};
            reachedSupport = FindGroundSupport(scene, kcc, descentStart, surfaceStartY - currentPos.y, crossing, entity);
            if (reachedSupport)
            {
                currentPos.y = surfaceStartY - crossing.Drop;
                setGroundContact(crossing.Contact);
                onSteepSlope = !kcc.IsGrounded;
                const f32 intoSupport = glm::dot(kcc.Velocity, kcc.GroundNormal);
                if (intoSupport < 0.0f)
                    kcc.Velocity -= kcc.GroundNormal * intoSupport;
            }
        }

        // 계단 내려가기 (Step-Down Ground Snapping: 점프 상승 중에는 실행 방지)
        if (kcc.IsGrounded && !justJumped && !stepped && !reachedSupport && kcc.StepOffset > 0.001f)
        {
            GroundSupport stepDown{};
            if (FindGroundSupport(scene, kcc, currentPos, GroundSnapDistance(kcc), stepDown, entity))
            {
                const auto& stepDownHit = stepDown.Contact.Hit;
                if (IsWalkableNormal(kcc, stepDownHit.normal.y))
                {
                    applyGroundSnap(stepDown);
                }
            }
        }
    }

    // 6. 공중 수직 이동 및 천장/바닥 충돌. 경사면에서 이미 적용한 하강을 중복 적용하지 않는다.
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
            GroundSupport landing{};
            if (FindGroundSupport(scene, kcc, currentPos, -vertDelta, landing, entity))
            {
                currentPos.y -= landing.Drop;
                setGroundContact(landing.Contact);
                if (kcc.IsGrounded)
                {
                    kcc.Velocity.y = 0.0f;
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
            animator = TryGetEnabledComponent<AnimatorComponent>(world, kcc.TargetAnimatorEntity);
        }
        if (!animator)
        {
            // 자기 자신, 없으면 자식 계층(모델 루트)에서 찾는다.
            const entt::entity animatorEntity = FindAnimatorEntityInHierarchy(world, entity);
            if (animatorEntity != entt::null) animator = &world.get<AnimatorComponent>(animatorEntity);
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

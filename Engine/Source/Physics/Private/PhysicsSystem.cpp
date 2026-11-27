#include "PhysicsSystem.h"
#include "SpatialBroadphase.h"
#include "ComponentEnable.h"
#include "Logger.h"
#include <chrono>
#include <map>
#include "PhysicsComponent.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "HierarchyComponent.h"
#include "FlagComponent.h"
#include "Collision.h"
#include "Entity.h"
#include "TransformSystem.h"
#include "WorldTransformComponent.h"
#include "ColliderComponent.h"
#include "PhysicsMaterial.h"
#include "TerrainComponent.h"
#include "Mesh.h"
#include <glm/gtc/matrix_transform.hpp>
#include "TriggerCallbackComponent.h"
#include "CharacterControllerComponent.h"
#include "NativeScriptComponent.h"
#include "GraphSystem.h"
#include "GraphBindingComponent.h"

#pragma region Helper
using PhysicsShape2D = variant<Rect, Circle, Capsule>;

static mat3 GetPhysicsMatrix2D(const mat4& world)
{
    return mat3(
        vec3(world[0].x, world[0].y, 0.f),
        vec3(world[1].x, world[1].y, 0.f),
        vec3(world[3].x, world[3].y, 1.f));
}

static bool TryBuildPhysicsShape(const entt::registry& world, entt::entity entity, PhysicsShape2D& result)
{
    const auto* activeCollider = TryGetEnabledComponent<Collider2DComponent>(world, entity);
    if (!activeCollider || !activeCollider->GetSettings().Enabled)
        return false;
    const auto& collider = *activeCollider;

    const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
    if (hierarchy && hierarchy->parent != entt::null)
        return false;

    const auto* flag = world.try_get<FlagComponent>(entity);
    if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
        HasFlag(flag->flags, EEntityFlag::Dead)))
        return false;

    const auto& worldTransform = world.get<WorldTransformComponent>(entity);
    const glm::dmat4 matrix(worldTransform.worldMatrix);
    const glm::dvec2 axisX(matrix[0]);
    const glm::dvec2 axisY(matrix[1]);
    const double scaleX = glm::length(axisX);
    const double scaleY = glm::length(axisY);
    const double limit = std::numeric_limits<f32>::max();
    constexpr double tolerance = 1.0e-6;

    if (!std::isfinite(scaleX) || !std::isfinite(scaleY) || scaleX <= 0.0 || scaleY <= 0.0 ||
        std::abs(matrix[0].z) > scaleX * tolerance ||
        std::abs(matrix[1].z) > scaleY * tolerance)
        return false;

    const glm::dvec2 offset(collider.GetOffset());
    const glm::dvec2 worldCenter(matrix * glm::dvec4(offset, 0.0, 1.0));

    for (int axis = 0; axis < 2; ++axis)
    {
        if (!std::isfinite(worldCenter[axis]) || std::abs(worldCenter[axis]) > limit)
            return false;
    }

    if (const auto* box = std::get_if<Box2DShape>(&collider.GetShape()))
    {
        if (!box->HasValidParameters())
            return false;

        const vec2 halfSize = box->Size * 0.5f;
        const vec2 minimum = collider.GetOffset() - halfSize;
        const Rect bounds(minimum.x, minimum.y, box->Size.x, box->Size.y);

        if (halfSize.x <= 0.f || halfSize.y <= 0.f || !IsFinite(minimum) ||
            !std::isfinite(bounds.Right()) || !std::isfinite(bounds.Bottom()))
            return false;

        for (int index = 0; index < 4; ++index)
        {
            const glm::dvec4 corner((index & 1) ? bounds.Right() : bounds.left,
                (index & 2) ? bounds.Bottom() : bounds.top, 0.0, 1.0);
            const glm::dvec2 point(matrix * corner);

            for (int axis = 0; axis < 2; ++axis)
            {
                if (!std::isfinite(point[axis]) || std::abs(point[axis]) > limit)
                    return false;
            }
        }

        result = bounds;
        return true;
    }

    // 원과 캡슐의 둥근 부분이 원으로 유지되는 변환만 지원합니다.
    if (std::abs(scaleX - scaleY) > tolerance * glm::max(scaleX, scaleY) ||
        std::abs(glm::dot(axisX / scaleX, axisY / scaleY)) > tolerance)
        return false;

    const auto* circle = std::get_if<Circle2DShape>(&collider.GetShape());
    const auto* capsule = std::get_if<Capsule2DShape>(&collider.GetShape());

    if (circle && !circle->HasValidParameters())
        return false;
    if (!circle && (!capsule || !capsule->HasValidParameters()))
        return false;

    const double localRadius = circle ? circle->Radius : capsule->Radius;
    const double scaledRadius = localRadius * ((scaleX + scaleY) * 0.5);
    if (!std::isfinite(scaledRadius) || scaledRadius <= 0.0 || scaledRadius > limit)
        return false;

    const f32 radius = static_cast<f32>(scaledRadius);
    if (radius <= 0.f)
        return false;

    if (circle)
    {
        result = Circle(vec2(worldCenter), radius);
        return true;
    }

    const double halfSegment = static_cast<double>(capsule->Height) * 0.5 - capsule->Radius;
    const glm::dvec2 segmentOffset = axisY * halfSegment;
    const glm::dvec2 first = worldCenter - segmentOffset;
    const glm::dvec2 second = worldCenter + segmentOffset;

    for (int axis = 0; axis < 2; ++axis)
    {
        if (!std::isfinite(first[axis]) || std::abs(first[axis]) > limit ||
            !std::isfinite(second[axis]) || std::abs(second[axis]) > limit)
            return false;
    }

    result = Capsule(vec3(vec2(first), 0.f), vec3(vec2(second), 0.f), radius);
    return true;
}

static bool IntersectsPhysicsShapes2D(const entt::registry& world, entt::entity entityA, entt::entity entityB, const PhysicsShape2D& shapeA, const PhysicsShape2D& shapeB, HitResult& hit)
{
    const mat4& worldA = world.get<WorldTransformComponent>(entityA).worldMatrix;
    const mat4& worldB = world.get<WorldTransformComponent>(entityB).worldMatrix;

    return std::visit([&](const auto& a, const auto& b)
        {
            using ShapeA = std::decay_t<decltype(a)>;
            using ShapeB = std::decay_t<decltype(b)>;

            if constexpr (std::is_same_v<ShapeA, Rect> && std::is_same_v<ShapeB, Rect>)
                return Collision::Intersects(a, GetPhysicsMatrix2D(worldA), b, GetPhysicsMatrix2D(worldB), hit);
            else if constexpr (std::is_same_v<ShapeA, Rect>)
                return Collision::Intersects(a, GetPhysicsMatrix2D(worldA), b, hit);
            else if constexpr (std::is_same_v<ShapeB, Rect>)
                return Collision::Intersects(a, b, GetPhysicsMatrix2D(worldB), hit);
            else if constexpr (std::is_same_v<ShapeA, Capsule> && std::is_same_v<ShapeB, Capsule>)
                return Collision::Intersects2D(a, b, hit);
            else
                return Collision::Intersects(a, b, hit);
        }, shapeA, shapeB);
}

static void ComputePhysicsShapeWorldAABB2D(const entt::registry& world, entt::entity entity,
    const PhysicsShape2D& shape, vec2& outMin, vec2& outMax)
{
    if (const auto* r = std::get_if<Rect>(&shape))
    {
        const mat4& worldMatrix = world.get<WorldTransformComponent>(entity).worldMatrix;
        outMin = vec2(std::numeric_limits<f32>::max());
        outMax = vec2(-std::numeric_limits<f32>::max());
        for (int index = 0; index < 4; ++index)
        {
            const f32 x = (index & 1) ? r->Right() : r->left;
            const f32 y = (index & 2) ? r->Bottom() : r->top;
            const vec2 pt(worldMatrix * vec4(x, y, 0.0f, 1.0f));
            outMin = glm::min(outMin, pt);
            outMax = glm::max(outMax, pt);
        }
    }
    else if (const auto* c = std::get_if<Circle>(&shape))
    {
        outMin = c->center - vec2(c->radius);
        outMax = c->center + vec2(c->radius);
    }
    else if (const auto* cap = std::get_if<Capsule>(&shape))
    {
        outMin = glm::min(vec2(cap->pointA), vec2(cap->pointB)) - vec2(cap->radius);
        outMax = glm::max(vec2(cap->pointA), vec2(cap->pointB)) + vec2(cap->radius);
    }
}

static void ResolvePhysicsContact(entt::registry& world, entt::entity entityA, entt::entity entityB, const HitResult& hit, bool resolveVelocity)
{
    auto* bodyA = TryGetEnabledComponent<RigidBody2DComponent>(world, entityA);
    auto* bodyB = TryGetEnabledComponent<RigidBody2DComponent>(world, entityB);

    const auto tryGetInverseMass = [](const RigidBody2DComponent* body, double& inverseMass)
        {
            inverseMass = 0.0;
            if (!body || body->BodyType == ERigidBodyType::Static)
                return true;

            if (body->BodyType != ERigidBodyType::Dynamic &&
                body->BodyType != ERigidBodyType::Kinematic)
                return false;

            if (!std::isfinite(body->LinearVelocity.x) || !std::isfinite(body->LinearVelocity.y))
                return false;

            if (body->BodyType == ERigidBodyType::Kinematic)
                return true;

            if (!std::isfinite(body->Mass) || body->Mass <= 0.f)
                return false;

            inverseMass = 1.0 / static_cast<double>(body->Mass);
            return true;
        };

    double inverseMassA, inverseMassB;
    if (!tryGetInverseMass(bodyA, inverseMassA) || !tryGetInverseMass(bodyB, inverseMassB))
        return;

    const double inverseMassSum = inverseMassA + inverseMassB;
    if (inverseMassSum <= 0.0)
        return;

    if (!hit.hasHit || !std::isfinite(hit.denetrationDepth) || hit.denetrationDepth < 0.f ||
        !std::isfinite(hit.normal.x) || !std::isfinite(hit.normal.y))
        return;

    // Collision의 법선은 A를 B 밖으로 밀어내는 단위 벡터입니다.
    const glm::dvec2 normal(hit.normal.x, hit.normal.y);
    const double weightA = inverseMassA / inverseMassSum;
    const double weightB = inverseMassB / inverseMassSum;

    const auto getContactVelocity = [](const RigidBody2DComponent* body) -> glm::dvec2
        {
            if (!body || body->BodyType == ERigidBodyType::Static)
                return glm::dvec2(0.0);

            return glm::dvec2(body->LinearVelocity);
        };

    glm::dvec2 velocityA = getContactVelocity(bodyA);
    glm::dvec2 velocityB = getContactVelocity(bodyB);
    const double normalSpeed = glm::dot(velocityA - velocityB, normal);

    const auto& settingsA = world.get<Collider2DComponent>(entityA).GetSettings();
    const auto& settingsB = world.get<Collider2DComponent>(entityB).GetSettings();

    const auto isValidRestitution = [](f32 value)
        {
            return std::isfinite(value) && value >= 0.f && value <= 1.f;
        };

    if (!isValidRestitution(settingsA.Restitution) || !isValidRestitution(settingsB.Restitution))
        return;

    if (!std::isfinite(settingsA.Friction) || settingsA.Friction < 0.f ||
        !std::isfinite(settingsB.Friction) || settingsB.Friction < 0.f)
        return;

    const double friction = std::sqrt(static_cast<double>(settingsA.Friction) * settingsB.Friction);

    double restitution = glm::max(settingsA.Restitution, settingsB.Restitution);

    // 낮은 접근 속도에서는 반발을 생략해 바닥 위의 작은 튀기를 줄입니다.
    constexpr double restitutionThreshold = 1.0;
    if (-normalSpeed < restitutionThreshold)
        restitution = 0.0;

    if (resolveVelocity && normalSpeed < 0.0)
    {
        const double normalVelocityChange = -(1.0 + restitution) * normalSpeed;
        velocityA += normal * normalVelocityChange * weightA;
        velocityB -= normal * normalVelocityChange * weightB;

        // 법선에 수직인 접선 방향의 상대속도를 줄입니다.
        const glm::dvec2 tangent(-normal.y, normal.x);
        const double tangentSpeed = glm::dot(velocityA - velocityB, tangent);
        const double frictionLimit = friction * normalVelocityChange;
        const double tangentVelocityChange = glm::clamp(-tangentSpeed, -frictionLimit, frictionLimit);

        velocityA += tangent * tangentVelocityChange * weightA;
        velocityB -= tangent * tangentVelocityChange * weightB;
    }

    const vec2 nextVelocityA(velocityA);
    const vec2 nextVelocityB(velocityB);
    if (!std::isfinite(nextVelocityA.x) || !std::isfinite(nextVelocityA.y) ||
        !std::isfinite(nextVelocityB.x) || !std::isfinite(nextVelocityB.y))
        return;

    auto& transformA = world.get<TransformComponent>(entityA);
    auto& transformB = world.get<TransformComponent>(entityB);
    auto candidateA = transformA;
    auto candidateB = transformB;

    constexpr double penetrationSlop = 0.0001;
    constexpr double correctionRatio = 0.8;
    const double correctionDepth = glm::max(static_cast<double>(hit.denetrationDepth) - penetrationSlop, 0.0);
    const glm::dvec2 correction = normal * correctionDepth * correctionRatio;
    const vec3 positionA(glm::dvec3(transformA.position) + glm::dvec3(correction * weightA, 0.0));
    const vec3 positionB(glm::dvec3(transformB.position) - glm::dvec3(correction * weightB, 0.0));

    if ((inverseMassA > 0.0 && !candidateA.SetLocalPosition(positionA)) ||
        (inverseMassB > 0.0 && !candidateB.SetLocalPosition(positionB)))
        return;

    auto candidateWorldA = world.get<WorldTransformComponent>(entityA);
    auto candidateWorldB = world.get<WorldTransformComponent>(entityB);

    if ((inverseMassA > 0.0 && !TransformSystem::UpdateRootWorld(candidateA, candidateWorldA)) ||
        (inverseMassB > 0.0 && !TransformSystem::UpdateRootWorld(candidateB, candidateWorldB)))
        return;

    if (inverseMassA > 0.0)
    {
        transformA = candidateA;
        world.get<WorldTransformComponent>(entityA) = candidateWorldA;
        bodyA->LinearVelocity = nextVelocityA;
    }

    if (inverseMassB > 0.0)
    {
        transformB = candidateB;
        world.get<WorldTransformComponent>(entityB) = candidateWorldB;
        bodyB->LinearVelocity = nextVelocityB;
    }
}

static bool ResolvePhysicsContact3D(entt::registry& world, entt::entity entityA, entt::entity entityB, const HitResult& hit, bool resolveVelocity, CollisionEvent* outCollision = nullptr)
{
    auto* bodyA = TryGetEnabledComponent<RigidBodyComponent>(world, entityA);
    auto* bodyB = TryGetEnabledComponent<RigidBodyComponent>(world, entityB);

    const auto tryGetInverseMass3D = [](const RigidBodyComponent* body, double& inverseMass)
    {
        inverseMass = 0.0;
        if (!body || body->BodyType == ERigidBodyType::Static)
            return true;
        if (body->BodyType != ERigidBodyType::Dynamic && body->BodyType != ERigidBodyType::Kinematic)
            return false;
        if (!std::isfinite(body->LinearVelocity.x) || !std::isfinite(body->LinearVelocity.y) || !std::isfinite(body->LinearVelocity.z))
            return false;
        if (body->BodyType == ERigidBodyType::Kinematic)
            return true;
        if (!std::isfinite(body->Mass) || body->Mass <= 0.f)
            return false;
        inverseMass = 1.0 / static_cast<double>(body->Mass);
        return true;
    };

    double inverseMassA = 0.0, inverseMassB = 0.0;
    if (!tryGetInverseMass3D(bodyA, inverseMassA) || !tryGetInverseMass3D(bodyB, inverseMassB))
        return false;

    const double inverseMassSum = inverseMassA + inverseMassB;
    if (inverseMassSum <= 0.0)
        return false;

    if (!hit.hasHit || !std::isfinite(hit.denetrationDepth) || hit.denetrationDepth < 0.f ||
        !std::isfinite(hit.normal.x) || !std::isfinite(hit.normal.y) || !std::isfinite(hit.normal.z))
        return false;

    const glm::dvec3 normal(hit.normal.x, hit.normal.y, hit.normal.z);
    const double weightA = inverseMassA / inverseMassSum;
    const double weightB = inverseMassB / inverseMassSum;

    const auto getContactVelocity3D = [](const RigidBodyComponent* body) -> glm::dvec3
    {
        if (!body || body->BodyType == ERigidBodyType::Static)
            return glm::dvec3(0.0);
        return glm::dvec3(body->LinearVelocity);
    };

    glm::dvec3 velocityA = getContactVelocity3D(bodyA);
    glm::dvec3 velocityB = getContactVelocity3D(bodyB);
    const double normalSpeed = glm::dot(velocityA - velocityB, normal);

    if (outCollision)
    {
        outCollision->entityA = entityA;
        outCollision->entityB = entityB;
        outCollision->contactPoint = hit.point;
        outCollision->contactNormal = hit.normal;
        outCollision->penetrationDepth = hit.denetrationDepth;
        outCollision->relativeVelocity = vec3(velocityA - velocityB);
        outCollision->type = ECollisionEventType::Enter;
    }

    const auto* colA = TryGetEnabledComponent<ColliderComponent>(world, entityA);
    const auto* colB = TryGetEnabledComponent<ColliderComponent>(world, entityB);
    const PhysicsMaterial* matA = (colA && colA->Material.IsValid()) ? colA->Material.Get() : nullptr;
    const PhysicsMaterial* matB = (colB && colB->Material.IsValid()) ? colB->Material.Get() : nullptr;

    const f32 restA = matA ? matA->Restitution : (colA ? colA->GetSettings().Restitution : 0.0f);
    const f32 restB = matB ? matB->Restitution : (colB ? colB->GetSettings().Restitution : 0.0f);
    const f32 fricA = matA ? matA->DynamicFriction : (colA ? colA->GetSettings().Friction : 0.5f);
    const f32 fricB = matB ? matB->DynamicFriction : (colB ? colB->GetSettings().Friction : 0.5f);

    if (!std::isfinite(fricA) || fricA < 0.f || !std::isfinite(fricB) || fricB < 0.f ||
        !std::isfinite(restA) || !std::isfinite(restB))
        return false;

    double friction = 0.0;
    double restitution = 0.0;
    if (matA || matB)
    {
        const EPhysicsCombineMode frictionCombine = (matA && matB)
            ? static_cast<EPhysicsCombineMode>(std::max(static_cast<uint8>(matA->FrictionCombine), static_cast<uint8>(matB->FrictionCombine)))
            : (matA ? matA->FrictionCombine : matB->FrictionCombine);
        const EPhysicsCombineMode restitutionCombine = (matA && matB)
            ? static_cast<EPhysicsCombineMode>(std::max(static_cast<uint8>(matA->RestitutionCombine), static_cast<uint8>(matB->RestitutionCombine)))
            : (matA ? matA->RestitutionCombine : matB->RestitutionCombine);

        friction = std::max(0.0, static_cast<double>(PhysicsMaterial::CombineValues(fricA, fricB, frictionCombine)));
        restitution = std::clamp(static_cast<double>(PhysicsMaterial::CombineValues(restA, restB, restitutionCombine)), 0.0, 1.0);
    }
    else
    {
        friction = std::sqrt(static_cast<double>(fricA) * fricB);
        restitution = glm::max(restA, restB);
    }
    constexpr double restitutionThreshold = 0.5;
    if (-normalSpeed < restitutionThreshold)
        restitution = 0.0;

    if (resolveVelocity && normalSpeed < 0.0)
    {
        const double normalVelocityChange = -(1.0 + restitution) * normalSpeed;
        velocityA += normal * normalVelocityChange * weightA;
        velocityB -= normal * normalVelocityChange * weightB;

        const glm::dvec3 relVel = velocityA - velocityB;
        const glm::dvec3 tangentVel = relVel - normal * glm::dot(relVel, normal);
        const double tangentSpeed = glm::length(tangentVel);
        if (tangentSpeed > 1.0e-5)
        {
            const glm::dvec3 tangent = tangentVel / tangentSpeed;
            const double frictionLimit = friction * normalVelocityChange;
            const double tangentVelocityChange = glm::clamp(-tangentSpeed, -frictionLimit, frictionLimit);
            velocityA += tangent * tangentVelocityChange * weightA;
            velocityB -= tangent * tangentVelocityChange * weightB;
        }
    }

    const vec3 nextVelocityA(velocityA);
    const vec3 nextVelocityB(velocityB);
    if (!IsFinite(nextVelocityA) || !IsFinite(nextVelocityB))
        return false;

    constexpr double penetrationSlop = 0.001;
    constexpr double correctionRatio = 0.35;
    constexpr double maxCorrectionPerIteration = 0.15;
    const double correctionDepth = glm::max(static_cast<double>(hit.denetrationDepth) - penetrationSlop, 0.0);
    if (correctionDepth <= 0.0)
    {
        if (resolveVelocity && normalSpeed < 0.0)
        {
            if (inverseMassA > 0.0 && bodyA) bodyA->LinearVelocity = nextVelocityA;
            if (inverseMassB > 0.0 && bodyB) bodyB->LinearVelocity = nextVelocityB;
        }
        return true;
    }

    auto& transformA = world.get<TransformComponent>(entityA);
    auto& transformB = world.get<TransformComponent>(entityB);
    auto candidateA = transformA;
    auto candidateB = transformB;

    const double clampedCorrection = std::min(correctionDepth * correctionRatio, maxCorrectionPerIteration);
    const glm::dvec3 correction = normal * clampedCorrection;
    const vec3 positionA(glm::dvec3(transformA.position) + correction * weightA);
    const vec3 positionB(glm::dvec3(transformB.position) - correction * weightB);

    if ((inverseMassA > 0.0 && !candidateA.SetLocalPosition(positionA)) ||
        (inverseMassB > 0.0 && !candidateB.SetLocalPosition(positionB)))
        return false;

    auto candidateWorldA = world.get<WorldTransformComponent>(entityA);
    auto candidateWorldB = world.get<WorldTransformComponent>(entityB);

    if ((inverseMassA > 0.0 && !TransformSystem::UpdateRootWorld(candidateA, candidateWorldA)) ||
        (inverseMassB > 0.0 && !TransformSystem::UpdateRootWorld(candidateB, candidateWorldB)))
        return false;

    if (inverseMassA > 0.0)
    {
        transformA = candidateA;
        world.get<WorldTransformComponent>(entityA) = candidateWorldA;
        if (bodyA) bodyA->LinearVelocity = nextVelocityA;
    }
    if (inverseMassB > 0.0)
    {
        transformB = candidateB;
        world.get<WorldTransformComponent>(entityB) = candidateWorldB;
        if (bodyB) bodyB->LinearVelocity = nextVelocityB;
    }
    return true;
}

static void ResolveJoints2D(entt::registry& world, f32 dt)
{
    (void)dt;
    auto view = world.view<Joint2DComponent, TransformComponent, WorldTransformComponent>();
    for (auto entityA : view)
    {
        const auto& joint = view.get<Joint2DComponent>(entityA);
        if (!IsComponentEnabled(world, entityA, joint) || joint.RestLength < 0.0f) continue;

        auto* bodyA = TryGetEnabledComponent<RigidBody2DComponent>(world, entityA);
        const auto connectedEntity = joint.ConnectedEntity;
        auto* bodyB = (connectedEntity != entt::null && world.valid(connectedEntity)) ? TryGetEnabledComponent<RigidBody2DComponent>(world, connectedEntity) : nullptr;

        const f32 invMassA = (bodyA && bodyA->BodyType == ERigidBodyType::Dynamic && bodyA->Mass > 0.0f) ? (1.0f / bodyA->Mass) : 0.0f;
        const f32 invMassB = (bodyB && bodyB->BodyType == ERigidBodyType::Dynamic && bodyB->Mass > 0.0f) ? (1.0f / bodyB->Mass) : 0.0f;
        const f32 invMassSum = invMassA + invMassB;
        if (invMassSum <= 1.0e-6f) continue;

        const auto& worldTransformA = view.get<WorldTransformComponent>(entityA);
        const vec2 anchorA = vec2(worldTransformA.worldMatrix * vec4(joint.LocalAnchor, 0.0f, 1.0f));

        vec2 anchorB = joint.ConnectedLocalAnchor;
        if (connectedEntity != entt::null && world.valid(connectedEntity) && world.all_of<WorldTransformComponent>(connectedEntity))
        {
            anchorB = vec2(world.get<WorldTransformComponent>(connectedEntity).worldMatrix * vec4(joint.ConnectedLocalAnchor, 0.0f, 1.0f));
        }

        vec2 delta = anchorA - anchorB;
        f32 dist = glm::length(delta);
        if (dist < 1.0e-5f)
        {
            delta = vec2(0.0f, 1.0f);
            dist = 1.0e-5f;
        }

        const vec2 n = delta / dist;
        const f32 C = dist - joint.RestLength;
        const vec2 corr = (C / invMassSum) * n;

        auto& trA = view.get<TransformComponent>(entityA);
        if (invMassA > 0.0f)
        {
            trA.position.x -= corr.x * invMassA;
            trA.position.y -= corr.y * invMassA;
            auto trWorldA = view.get<WorldTransformComponent>(entityA);
            if (TransformSystem::UpdateRootWorld(trA, trWorldA))
                world.get<WorldTransformComponent>(entityA) = trWorldA;
        }

        if (invMassB > 0.0f && connectedEntity != entt::null && world.valid(connectedEntity) && world.all_of<TransformComponent, WorldTransformComponent>(connectedEntity))
        {
            auto& trB = world.get<TransformComponent>(connectedEntity);
            trB.position.x += corr.x * invMassB;
            trB.position.y += corr.y * invMassB;
            auto trWorldB = world.get<WorldTransformComponent>(connectedEntity);
            if (TransformSystem::UpdateRootWorld(trB, trWorldB))
                world.get<WorldTransformComponent>(connectedEntity) = trWorldB;
        }

        const vec2 velA = (bodyA && bodyA->BodyType == ERigidBodyType::Dynamic) ? bodyA->LinearVelocity : vec2(0.0f);
        const vec2 velB = (bodyB && bodyB->BodyType == ERigidBodyType::Dynamic) ? bodyB->LinearVelocity : vec2(0.0f);
        const f32 relSpeed = glm::dot(velA - velB, n);
        const f32 impulse = (relSpeed / invMassSum) * 0.5f;
        if (bodyA && invMassA > 0.0f) bodyA->LinearVelocity -= n * (impulse * invMassA);
        if (bodyB && invMassB > 0.0f) bodyB->LinearVelocity += n * (impulse * invMassB);
    }
}

static void ResolveJoints3D(entt::registry& world, f32 dt)
{
    (void)dt;
    auto view = world.view<JointComponent, TransformComponent, WorldTransformComponent>();
    for (auto entityA : view)
    {
        const auto& joint = view.get<JointComponent>(entityA);
        if (!IsComponentEnabled(world, entityA, joint) || joint.RestLength < 0.0f) continue;

        auto* bodyA = TryGetEnabledComponent<RigidBodyComponent>(world, entityA);
        const auto connectedEntity = joint.ConnectedEntity;
        auto* bodyB = (connectedEntity != entt::null && world.valid(connectedEntity)) ? TryGetEnabledComponent<RigidBodyComponent>(world, connectedEntity) : nullptr;

        const f32 invMassA = (bodyA && bodyA->BodyType == ERigidBodyType::Dynamic && bodyA->Mass > 0.0f) ? (1.0f / bodyA->Mass) : 0.0f;
        const f32 invMassB = (bodyB && bodyB->BodyType == ERigidBodyType::Dynamic && bodyB->Mass > 0.0f) ? (1.0f / bodyB->Mass) : 0.0f;
        const f32 invMassSum = invMassA + invMassB;
        if (invMassSum <= 1.0e-6f) continue;

        const auto& worldTransformA = view.get<WorldTransformComponent>(entityA);
        const vec3 anchorA = vec3(worldTransformA.worldMatrix * vec4(joint.LocalAnchor, 1.0f));

        vec3 anchorB = joint.ConnectedLocalAnchor;
        if (connectedEntity != entt::null && world.valid(connectedEntity) && world.all_of<WorldTransformComponent>(connectedEntity))
        {
            anchorB = vec3(world.get<WorldTransformComponent>(connectedEntity).worldMatrix * vec4(joint.ConnectedLocalAnchor, 1.0f));
        }

        vec3 delta = anchorA - anchorB;
        f32 dist = glm::length(delta);
        if (dist < 1.0e-5f)
        {
            delta = vec3(0.0f, 1.0f, 0.0f);
            dist = 1.0e-5f;
        }

        const vec3 n = delta / dist;
        const f32 C = dist - joint.RestLength;
        const vec3 corr = (C / invMassSum) * n;

        auto& trA = view.get<TransformComponent>(entityA);
        if (invMassA > 0.0f)
        {
            trA.position -= corr * invMassA;
            auto trWorldA = view.get<WorldTransformComponent>(entityA);
            if (TransformSystem::UpdateRootWorld(trA, trWorldA))
                world.get<WorldTransformComponent>(entityA) = trWorldA;
        }

        if (invMassB > 0.0f && connectedEntity != entt::null && world.valid(connectedEntity) && world.all_of<TransformComponent, WorldTransformComponent>(connectedEntity))
        {
            auto& trB = world.get<TransformComponent>(connectedEntity);
            trB.position += corr * invMassB;
            auto trWorldB = world.get<WorldTransformComponent>(connectedEntity);
            if (TransformSystem::UpdateRootWorld(trB, trWorldB))
                world.get<WorldTransformComponent>(connectedEntity) = trWorldB;
        }

        const vec3 velA = (bodyA && bodyA->BodyType == ERigidBodyType::Dynamic) ? bodyA->LinearVelocity : vec3(0.0f);
        const vec3 velB = (bodyB && bodyB->BodyType == ERigidBodyType::Dynamic) ? bodyB->LinearVelocity : vec3(0.0f);
        const f32 relSpeed = glm::dot(velA - velB, n);
        const f32 impulse = (relSpeed / invMassSum) * 0.5f;
        if (bodyA && invMassA > 0.0f) bodyA->LinearVelocity -= n * (impulse * invMassA);
        if (bodyB && invMassB > 0.0f) bodyB->LinearVelocity += n * (impulse * invMassB);
    }
}

using PhysicsTriggerPair = pair<entt::entity, entt::entity>;
struct PhysicsTriggerState
{
    set<PhysicsTriggerPair> Previous;
    vector<TriggerEvent> Events;
};
struct PhysicsTriggerState3D
{
    set<PhysicsTriggerPair> Previous;
    vector<TriggerEvent> Events;
};
static void CollectPhysicsTriggerEvents2D(Scene& scene);
static void CollectPhysicsTriggerEvents3D(Scene& scene);
namespace
{
    std::vector<PhysicsSystem::GlobalTriggerCallback> s_GlobalTriggerCallbacks;
    std::vector<PhysicsSystem::GlobalCollisionCallback> s_GlobalCollisionCallbacks;
    Engine::PhysicsSettings s_PhysicsSettings;

    using PhysicsCollisionPair = std::pair<entt::entity, entt::entity>;
    struct PhysicsCollisionState3D
    {
        std::set<PhysicsCollisionPair> Previous;
        std::vector<CollisionEvent> Events;
    };

    void FinalizePhysicsCollisionEvents3D(Scene& scene, const std::map<PhysicsCollisionPair, CollisionEvent>& currentContacts)
    {
        auto& world = scene.GetRegistry();
        auto& state = world.ctx().emplace<PhysicsCollisionState3D>();
        state.Events.clear();

        std::set<PhysicsCollisionPair> currentPairs;
        for (const auto& [pair, contact] : currentContacts)
        {
            currentPairs.insert(pair);
            CollisionEvent event = contact;
            event.type = state.Previous.contains(pair) ? ECollisionEventType::Stay : ECollisionEventType::Enter;
            state.Events.push_back(event);
        }

        for (const auto& pair : state.Previous)
        {
            if (currentPairs.contains(pair))
                continue;

            if (scene.FindEntity(pair.first) || scene.FindEntity(pair.second))
            {
                CollisionEvent exitEvent{};
                exitEvent.entityA = pair.first;
                exitEvent.entityB = pair.second;
                exitEvent.type = ECollisionEventType::Exit;
                state.Events.push_back(exitEvent);
            }
        }

        state.Previous = std::move(currentPairs);
    }
}

const Engine::PhysicsSettings& Engine::PhysicsSystem::GetSettings()
{
    return s_PhysicsSettings;
}

void Engine::PhysicsSystem::SetSettings(const Engine::PhysicsSettings& settings)
{
    s_PhysicsSettings = settings;
}

static void CollectPhysicsTriggerEvents2D(Scene& scene)
{
    auto& world = scene.GetRegistry();
    auto& state = world.ctx().emplace<PhysicsTriggerState>();
    state.Events.clear();

    std::set<PhysicsTriggerPair> current;
    auto colliders = world.view<Collider2DComponent, TransformComponent, WorldTransformComponent>();

    SpatialBroadphase2D broadphase2D;
    for (auto entity : colliders)
    {
        PhysicsShape2D shape;
        if (!TryBuildPhysicsShape(world, entity, shape)) continue;

        vec2 bMin(0.0f), bMax(0.0f);
        ComputePhysicsShapeWorldAABB2D(world, entity, shape, bMin, bMax);
        broadphase2D.Insert(entity, bMin, bMax);
    }

    std::vector<std::pair<entt::entity, entt::entity>> candidatePairs;
    broadphase2D.QueryPairs(candidatePairs);

    for (const auto& [handleA, handleB] : candidatePairs)
    {
        const auto& settingsA = colliders.get<Collider2DComponent>(handleA).GetSettings();
        const auto& settingsB = colliders.get<Collider2DComponent>(handleB).GetSettings();

        if (!settingsA.IsTrigger && !settingsB.IsTrigger)
            continue;

        auto* entityA = scene.FindEntity(handleA);
        auto* entityB = scene.FindEntity(handleB);
        HitResult hit{};

        if (!entityA || !entityB || !PhysicsSystem::Overlap2D(*entityA, *entityB, hit))
            continue;

        PhysicsTriggerPair pair{ handleA, handleB };
        if (pair.second < pair.first)
            std::swap(pair.first, pair.second);

        current.insert(pair);
    }

    for (const auto& pair : current)
    {
        const auto type = state.Previous.contains(pair)
            ? ETriggerEventType::Stay
            : ETriggerEventType::Enter;

        state.Events.push_back({ pair.first, pair.second, type });
    }

    for (const auto& pair : state.Previous)
    {
        if (current.contains(pair))
            continue;

        // 한쪽이 삭제돼도 살아 있는 쪽에서 이탈을 처리할 수 있습니다.
        if (scene.FindEntity(pair.first) || scene.FindEntity(pair.second))
            state.Events.push_back({ pair.first, pair.second, ETriggerEventType::Exit });
    }

    state.Previous = std::move(current);
}
static bool ComputeColliderWorldAABB3D(const ColliderComponent& col, const mat4& worldMatrix, BoundingBox& outAABB)
{
    const vec3 worldCenter = vec3(worldMatrix * vec4(col.GetOffset(), 1.0f));
    const vec3 col0 = vec3(worldMatrix[0]);
    const vec3 col1 = vec3(worldMatrix[1]);
    const vec3 col2 = vec3(worldMatrix[2]);

    if (const auto* box = std::get_if<BoxShape>(&col.GetShape()))
    {
        if (!box->HasValidParameters()) return false;
        const vec3 halfSize = box->Size * 0.5f;
        const vec3 extent = glm::abs(col0) * halfSize.x + glm::abs(col1) * halfSize.y + glm::abs(col2) * halfSize.z;
        outAABB = BoundingBox(worldCenter - extent, worldCenter + extent);
        return true;
    }
    if (const auto* sphere = std::get_if<SphereShape>(&col.GetShape()))
    {
        if (!sphere->HasValidParameters()) return false;
        const f32 maxScale = glm::max(glm::length(col0), glm::max(glm::length(col1), glm::length(col2)));
        const vec3 extent(sphere->Radius * maxScale);
        outAABB = BoundingBox(worldCenter - extent, worldCenter + extent);
        return true;
    }
    if (const auto* cap = std::get_if<CapsuleShape>(&col.GetShape()))
    {
        if (!cap->HasValidParameters()) return false;
        const f32 halfSegment = std::max(0.0f, (cap->Height * 0.5f) - cap->Radius);
        const f32 scaleXZ = glm::max(glm::length(col0), glm::length(col2));
        const vec3 extent = glm::abs(col1) * halfSegment + vec3(cap->Radius * scaleXZ);
        outAABB = BoundingBox(worldCenter - extent, worldCenter + extent);
        return true;
    }
    return false;
}

static void CollectPhysicsTriggerEvents3D(Scene& scene)
{
    auto& world = scene.GetRegistry();
    auto& state = world.ctx().emplace<PhysicsTriggerState3D>();
    state.Events.clear();

    struct TriggerCandidate3D
    {
        entt::entity entity;
        uint32 layer;
        uint32 mask;
        bool isTrigger;
        BoundingBox worldAABB;
    };

    auto colliders = world.view<ColliderComponent, WorldTransformComponent>();
    std::vector<TriggerCandidate3D> allColliders;
    std::vector<size_t> triggerIndices;

    for (auto entity : colliders)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = colliders.get<ColliderComponent>(entity);
        const auto& settings = col.GetSettings();
        if (!settings.Enabled) continue;

        const auto& tr = colliders.get<WorldTransformComponent>(entity);
        BoundingBox aabb{};
        if (!ComputeColliderWorldAABB3D(col, tr.worldMatrix, aabb)) continue;

        if (settings.IsTrigger)
            triggerIndices.push_back(allColliders.size());

        allColliders.push_back({ entity, settings.CollisionLayer, settings.CollisionMask, settings.IsTrigger, aabb });
    }

    std::set<PhysicsTriggerPair> current;
    if (!triggerIndices.empty())
    {
        SpatialBroadphase3D broadphase3D;
        for (size_t i = 0; i < allColliders.size(); ++i)
        {
            broadphase3D.Insert(static_cast<entt::entity>(i), allColliders[i].worldAABB);
        }

        for (size_t tIdx : triggerIndices)
        {
            const auto& trig = allColliders[tIdx];
            std::vector<entt::entity> candidates;
            broadphase3D.QueryAABB(trig.worldAABB, candidates);

            for (entt::entity candEntity : candidates)
            {
                const size_t oIdx = static_cast<size_t>(candEntity);
                if (tIdx == oIdx) continue;
                const auto& other = allColliders[oIdx];
                if (other.isTrigger && oIdx < tIdx) continue;
                if ((trig.layer & other.mask) == 0u || (other.layer & trig.mask) == 0u) continue;

                HitResult hit{};
                if (!PhysicsSystem::Overlap3D(scene, trig.entity, other.entity, hit)) continue;
                PhysicsTriggerPair pair{ trig.entity, other.entity };
                if (pair.second < pair.first) std::swap(pair.first, pair.second);
                current.insert(pair);
            }
        }

        auto kccView = world.view<CharacterControllerComponent, WorldTransformComponent>();
        for (auto kccEntity : kccView)
        {
            if (!IsComponentEnabled<CharacterControllerComponent>(world, kccEntity)) continue;
            const auto& kcc = kccView.get<CharacterControllerComponent>(kccEntity);
            const auto& kccTr = kccView.get<WorldTransformComponent>(kccEntity);
            const BoundingBox kccAABB = kcc.GetWorldAABB(vec3(kccTr.worldMatrix[3]));

            std::vector<entt::entity> candidates;
            broadphase3D.QueryAABB(kccAABB, candidates);

            for (entt::entity candEntity : candidates)
            {
                const size_t tIdx = static_cast<size_t>(candEntity);
                const auto& trig = allColliders[tIdx];
                if (!trig.isTrigger) continue;
                if ((trig.layer & kcc.CollisionMask) == 0u) continue;

                HitResult hit{};
                if (!PhysicsSystem::Overlap3D(scene, kccEntity, trig.entity, hit)) continue;
                PhysicsTriggerPair pair{ kccEntity, trig.entity };
                if (pair.second < pair.first) std::swap(pair.first, pair.second);
                current.insert(pair);
            }
        }
    }

    for (const auto& pair : current)
    {
        const auto type = state.Previous.contains(pair) ? ETriggerEventType::Stay : ETriggerEventType::Enter;
        state.Events.push_back({ pair.first, pair.second, type });
    }
    for (const auto& pair : state.Previous)
    {
        if (current.contains(pair)) continue;
        if (scene.FindEntity(pair.first) || scene.FindEntity(pair.second))
            state.Events.push_back({ pair.first, pair.second, ETriggerEventType::Exit });
    }
    state.Previous = std::move(current);
}
#pragma endregion

#pragma region Constructor&Destructor
EResult PhysicsSystem::Initialize(void* arg)
{
    (void)arg;
    return EResult::Success;
}

PhysicsSystem* PhysicsSystem::Create(void* arg)
{
    PhysicsSystem* instance = new PhysicsSystem();
    if (IsFailure(instance->Initialize(arg)))
    {
        Safe_Release(instance);
        return nullptr;
    }
    return instance;
}

void PhysicsSystem::Free()
{
}
#pragma endregion

#pragma region Loop
void PhysicsSystem::OnFixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
    // 첫 단계에서는 PIE 복원 대상인 씬의 강체만 처리합니다.
    (void)registry;

    if (!std::isfinite(dt) || dt <= 0.f)
        return;

    const vec2 gravity = s_PhysicsSettings.Gravity2D;

    for (Scene* scene : activeScenes)
    {
        if (!scene)
            continue;

        if (!scene->IsActive() || scene->IsPaused())
        {
            if (auto* state = scene->GetRegistry().ctx().find<PhysicsTriggerState>())
                state->Events.clear();

            continue;
        }

        auto& world = scene->GetRegistry();

        if (!TransformSystem::UpdateHierarchy(world))
        {
            if (auto* state = world.ctx().find<PhysicsTriggerState>())
                state->Events.clear();

            continue;
        }
        
        auto view = world.view<RigidBody2DComponent, TransformComponent, WorldTransformComponent>();

        for (auto entity : view)
        {
            if (!IsComponentEnabled<RigidBody2DComponent>(world, entity)) continue;
            auto& body = view.get<RigidBody2DComponent>(entity);
            auto& transform = view.get<TransformComponent>(entity);

            if (body.BodyType != ERigidBodyType::Dynamic &&
                body.BodyType != ERigidBodyType::Kinematic)
                continue;

            const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
            if (hierarchy && hierarchy->parent != entt::null)
                continue;

            const auto* flag = world.try_get<FlagComponent>(entity);
            if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
                HasFlag(flag->flags, EEntityFlag::Dead)))
                continue;

            // 외력은 이번 고정 스텝에서 소비합니다.
            const vec2 force = body.AccumulatedForce;
            body.AccumulatedForce = vec2(0.f);

            if (body.BodyType == ERigidBodyType::Kinematic)
            {
                if (!transform.HasValidAbsoluteFlags() ||
                    !std::isfinite(body.LinearVelocity.x) || !std::isfinite(body.LinearVelocity.y))
                    continue;

                const vec3 position = transform.position + vec3(body.LinearVelocity, 0.f) * dt;
                auto candidate = transform;
                auto candidateWorld = world.get<WorldTransformComponent>(entity);

                if (!candidate.SetLocalPosition(position) ||
                    !TransformSystem::UpdateRootWorld(candidate, candidateWorld))
                    continue;

                transform = candidate;
                world.get<WorldTransformComponent>(entity) = candidateWorld;
                continue;
            }

            if (!transform.HasValidAbsoluteFlags() ||
                !std::isfinite(body.Mass) || body.Mass <= 0.f ||
                !std::isfinite(body.LinearDamping) || body.LinearDamping < 0.f ||
                !std::isfinite(body.GravityScale) ||
                !std::isfinite(body.LinearVelocity.x) || !std::isfinite(body.LinearVelocity.y) ||
                !std::isfinite(force.x) || !std::isfinite(force.y))
                continue;

            const vec2 acceleration = gravity * body.GravityScale + force / body.Mass;
            if (!std::isfinite(acceleration.x) || !std::isfinite(acceleration.y))
                continue;

            const double dampingFactor = std::exp(-static_cast<double>(body.LinearDamping) * dt);
            const vec2 velocity = (body.LinearVelocity + acceleration * dt) * static_cast<f32>(dampingFactor);
            if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y))
                continue;

            const vec3 position = transform.position + vec3(velocity.x, velocity.y, 0.f) * dt;
            auto candidate = transform;
            auto candidateWorld = world.get<WorldTransformComponent>(entity);

            if (!candidate.SetLocalPosition(position) ||
                !TransformSystem::UpdateRootWorld(candidate, candidateWorld))
                continue;

            transform = candidate;
            world.get<WorldTransformComponent>(entity) = candidateWorld;
            body.LinearVelocity = velocity;

            // 2D 토크 소비 및 각속도 적분
            const f32 torque = body.AccumulatedTorque;
            body.AccumulatedTorque = 0.f;

            if (!body.FreezeRotation)
            {
                if (body.InverseInertia <= 0.f && body.Mass > 0.f)
                {
                    f32 width = 1.0f, height = 1.0f;
                    if (const auto* col2D = TryGetEnabledComponent<Collider2DComponent>(world, entity))
                    {
                        if (const auto* b2D = std::get_if<Box2DShape>(&col2D->GetShape()))
                        {
                            width = b2D->Size.x;
                            height = b2D->Size.y;
                        }
                        else if (const auto* c2D = std::get_if<Circle2DShape>(&col2D->GetShape()))
                        {
                            width = height = c2D->Radius * 2.0f;
                        }
                    }
                    const f32 inertia = (1.0f / 12.0f) * body.Mass * (width * width + height * height);
                    body.InverseInertia = inertia > 0.0f ? (1.0f / inertia) : 1.0f;
                }

                if (body.InverseInertia > 0.f)
                {
                    const f32 angularAccel = torque * body.InverseInertia;
                    const double angDampingFactor = std::exp(-static_cast<double>(body.AngularDamping) * dt);
                    body.AngularVelocity = (body.AngularVelocity + angularAccel * dt) * static_cast<f32>(angDampingFactor);
                    if (std::abs(body.AngularVelocity) > 1.0e-5f)
                    {
                        vec3 euler = candidate.eulerRotation;
                        euler.z += glm::degrees(body.AngularVelocity * dt);
                        if (candidate.SetEulerRotation(euler))
                        {
                            transform = candidate;
                            TransformSystem::UpdateRootWorld(transform, candidateWorld);
                            world.get<WorldTransformComponent>(entity) = candidateWorld;
                        }
                    }
                }
            }
        }

        // ── 3D 강체(RigidBodyComponent) 시뮬레이션 ──
        auto view3D = world.view<RigidBodyComponent, TransformComponent, WorldTransformComponent>();
        const vec3 gravity3D = s_PhysicsSettings.Gravity3D;

        for (auto entity : view3D)
        {
            if (!IsComponentEnabled<RigidBodyComponent>(world, entity)) continue;
            auto& body = view3D.get<RigidBodyComponent>(entity);
            auto& transform = view3D.get<TransformComponent>(entity);

            if (body.BodyType != ERigidBodyType::Dynamic &&
                body.BodyType != ERigidBodyType::Kinematic)
                continue;

            const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
            if (hierarchy && hierarchy->parent != entt::null)
                continue;

            const auto* flag = world.try_get<FlagComponent>(entity);
            if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
                HasFlag(flag->flags, EEntityFlag::Dead)))
                continue;

            const vec3 force = body.AccumulatedForce;
            const vec3 torque = body.AccumulatedTorque;
            body.AccumulatedForce = vec3(0.f);
            body.AccumulatedTorque = vec3(0.f);

            if (body.BodyType == ERigidBodyType::Kinematic)
            {
                if (!transform.HasValidAbsoluteFlags() || !IsFinite(body.LinearVelocity))
                    continue;

                const vec3 position = transform.position + body.LinearVelocity * dt;
                auto candidate = transform;
                auto candidateWorld = world.get<WorldTransformComponent>(entity);

                if (!candidate.SetLocalPosition(position) ||
                    !TransformSystem::UpdateRootWorld(candidate, candidateWorld))
                    continue;

                transform = candidate;
                world.get<WorldTransformComponent>(entity) = candidateWorld;
                continue;
            }

            if (!transform.HasValidAbsoluteFlags() ||
                !std::isfinite(body.Mass) || body.Mass <= 0.f ||
                !std::isfinite(body.LinearDamping) || body.LinearDamping < 0.f ||
                !std::isfinite(body.GravityScale) ||
                !IsFinite(body.LinearVelocity) || !IsFinite(force))
                continue;

            constexpr f32 kSleepLinearThreshold = 0.08f;
            constexpr f32 kSleepAngularThreshold = 0.08f;
            constexpr f32 kSleepDelay = 0.25f;

            const bool hasExternalInput = (glm::length(force) > 1.0e-4f) || (glm::length(torque) > 1.0e-4f);
            const bool isMoving = (glm::length(body.LinearVelocity) > kSleepLinearThreshold) ||
                                  (glm::length(body.AngularVelocity) > kSleepAngularThreshold);

            if (hasExternalInput || isMoving)
            {
                body.WakeUp();
            }
            else if (body.IsSleeping)
            {
                continue;
            }
            else
            {
                body.SleepTimer += dt;
                if (body.SleepTimer >= kSleepDelay)
                {
                    body.IsSleeping = true;
                    body.LinearVelocity = vec3(0.f);
                    body.AngularVelocity = vec3(0.f);
                    continue;
                }
            }

            const vec3 acceleration = gravity3D * body.GravityScale + force / body.Mass;
            if (!IsFinite(acceleration))
                continue;

            const double dampingFactor = std::exp(-static_cast<double>(body.LinearDamping) * dt);
            vec3 velocity = (body.LinearVelocity + acceleration * dt) * static_cast<f32>(dampingFactor);
            if (!IsFinite(velocity))
                continue;

            vec3 position = transform.position + velocity * dt;

            if (body.UseCCD && std::isfinite(body.CCDMotionThreshold) && body.CCDMotionThreshold >= 0.f)
            {
                const vec3 delta = velocity * dt;
                const f32 moveDist = glm::length(delta);
                if (std::isfinite(moveDist) && moveDist >= body.CCDMotionThreshold && moveDist > 1.0e-6f)
                {
                    if (const auto* col = TryGetEnabledComponent<ColliderComponent>(world, entity);
                        col && col->GetSettings().Enabled && !col->GetSettings().IsTrigger)
                    {
                        const mat4 colliderWorld = world.get<WorldTransformComponent>(entity).worldMatrix * glm::translate(mat4(1.0f), col->GetOffset());
                        const vec3 dir = delta / moveDist;
                        SweepQuery query{};
                        query.maxDistance = moveDist;
                        query.layerMask = col->GetSettings().CollisionMask;
                        query.includeTrigger = false;
                        query.ignoreEntity = entity;

                        SceneSweepHit sweepHit{};
                        bool hasSweepHit = false;
                        if (const auto* sph = std::get_if<SphereShape>(&col->GetShape()))
                        {
                            const f32 scale = glm::max(glm::length(vec3(colliderWorld[0])), glm::max(glm::length(vec3(colliderWorld[1])), glm::length(vec3(colliderWorld[2]))));
                            const f32 radius = sph->Radius * scale;
                            if (sph->HasValidParameters() && radius > 0.f)
                                hasSweepHit = PhysicsSystem::SweepSphere(*scene, vec3(colliderWorld[3]), radius, dir, sweepHit, query);
                        }
                        else if (const auto* box = std::get_if<BoxShape>(&col->GetShape()))
                        {
                            if (box->HasValidParameters())
                            {
                                const vec3 halfSize = box->Size * 0.5f;
                                const vec3 extent = glm::abs(vec3(colliderWorld[0])) * halfSize.x +
                                                    glm::abs(vec3(colliderWorld[1])) * halfSize.y +
                                                    glm::abs(vec3(colliderWorld[2])) * halfSize.z;
                                hasSweepHit = PhysicsSystem::SweepBox(*scene, vec3(colliderWorld[3]), extent, dir, sweepHit, query);
                            }
                        }
                        else if (const auto* cap = std::get_if<CapsuleShape>(&col->GetShape()))
                        {
                            const f32 halfSegment = std::max(0.0f, (cap->Height * 0.5f) - cap->Radius);
                            const f32 scaleXZ = glm::max(glm::length(vec3(colliderWorld[0])), glm::length(vec3(colliderWorld[2])));
                            const f32 radius = cap->Radius * scaleXZ;
                            if (cap->HasValidParameters() && radius > 0.f)
                            {
                                const vec3 pA = vec3(colliderWorld * vec4(0.0f, -halfSegment, 0.0f, 1.0f));
                                const vec3 pB = vec3(colliderWorld * vec4(0.0f, halfSegment, 0.0f, 1.0f));
                                hasSweepHit = PhysicsSystem::SweepCapsule(*scene, pA, pB, radius, dir, sweepHit, query);
                            }
                        }

                        if (hasSweepHit && sweepHit.Hit.hasHit && sweepHit.Hit.distance > 0.f && sweepHit.Hit.distance < moveDist)
                        {
                            const f32 clampedDist = std::min(moveDist, sweepHit.Hit.distance + 0.0005f);
                            position = transform.position + dir * clampedDist;
                        }
                    }
                }
            }


            auto candidate = transform;
            auto candidateWorld = world.get<WorldTransformComponent>(entity);

            if (!candidate.SetLocalPosition(position) ||
                !TransformSystem::UpdateRootWorld(candidate, candidateWorld))
                continue;

            if (!body.FreezeRotation)
            {
                const double angDamping = std::exp(-static_cast<double>(body.AngularDamping) * dt);
                body.AngularVelocity = (body.AngularVelocity + (torque / body.Mass) * dt) * static_cast<f32>(angDamping);
                if (glm::length(body.AngularVelocity) > 1.0e-5f)
                {
                    vec3 euler = candidate.eulerRotation + glm::degrees(body.AngularVelocity * dt);
                    if (candidate.SetEulerRotation(euler))
                    {
                        TransformSystem::UpdateRootWorld(candidate, candidateWorld);
                    }
                }
            }

            transform = candidate;
            world.get<WorldTransformComponent>(entity) = candidateWorld;
            body.LinearVelocity = velocity;
        }

        ResolveJoints2D(world, dt);
        ResolveJoints3D(world, dt);

        struct ColliderProxy3D
        {
            entt::entity entity;
            TransformComponent* transform;
            uint32 layer;
            uint32 mask;
            bool isDynamic;
            bool isBox;
            BoundingBox worldAABB;
        };

        auto colliders = world.view<Collider2DComponent, TransformComponent, WorldTransformComponent>();
        auto colliders3D = world.view<ColliderComponent, TransformComponent, WorldTransformComponent>();

        std::vector<ColliderProxy3D> proxies3D;
        for (auto entity : colliders3D)
        {
            if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
            const auto& col = colliders3D.get<ColliderComponent>(entity);
            const auto& settings = col.GetSettings();
            if (!settings.Enabled || settings.IsTrigger) continue;

            auto& tr = colliders3D.get<TransformComponent>(entity);
            const auto& worldTr = colliders3D.get<WorldTransformComponent>(entity);
            BoundingBox aabb{};
            if (!ComputeColliderWorldAABB3D(col, worldTr.worldMatrix, aabb)) continue;

            const auto* body = TryGetEnabledComponent<RigidBodyComponent>(world, entity);
            const bool isDyn = body && body->BodyType == ERigidBodyType::Dynamic;
            const bool isBox = std::holds_alternative<BoxShape>(col.GetShape());

            proxies3D.push_back({ entity, &tr, settings.CollisionLayer, settings.CollisionMask, isDyn, isBox, aabb });
        }

        const int positionIterations = glm::clamp(s_PhysicsSettings.PositionIterations, 1, 4);
        std::map<PhysicsCollisionPair, CollisionEvent> frameCollisions3D;

        for (int iteration = 0; iteration < positionIterations; ++iteration)
        {
            const bool resolveVelocity = iteration == 0;
            bool anyPenetration = false;

            SpatialBroadphase2D broadphase2D;
            for (auto entity : colliders)
            {
                const auto& settings = colliders.get<Collider2DComponent>(entity).GetSettings();
                if (settings.IsTrigger) continue;
                PhysicsShape2D shape;
                if (!TryBuildPhysicsShape(world, entity, shape)) continue;

                vec2 bMin(0.0f), bMax(0.0f);
                ComputePhysicsShapeWorldAABB2D(world, entity, shape, bMin, bMax);
                broadphase2D.Insert(entity, bMin, bMax);
            }

            std::vector<std::pair<entt::entity, entt::entity>> candidatePairs2D;
            broadphase2D.QueryPairs(candidatePairs2D);

            for (const auto& [entityA, entityB] : candidatePairs2D)
            {
                const auto& settingsA = colliders.get<Collider2DComponent>(entityA).GetSettings();
                const auto& settingsB = colliders.get<Collider2DComponent>(entityB).GetSettings();

                if (settingsA.IsTrigger || settingsB.IsTrigger)
                    continue;

                if ((settingsA.CollisionLayer & settingsB.CollisionMask) == 0u ||
                    (settingsB.CollisionLayer & settingsA.CollisionMask) == 0u)
                    continue;

                PhysicsShape2D shapeA, shapeB;
                if (!TryBuildPhysicsShape(world, entityA, shapeA) ||
                    !TryBuildPhysicsShape(world, entityB, shapeB))
                    continue;

                HitResult hit{};
                if (!IntersectsPhysicsShapes2D(world, entityA, entityB, shapeA, shapeB, hit))
                    continue;

                if (hit.denetrationDepth > 0.001f)
                    anyPenetration = true;

                ResolvePhysicsContact(world, entityA, entityB, hit, resolveVelocity);
            }

            const size_t numProxies = proxies3D.size();
            SpatialBroadphase3D broadphase3D;
            for (size_t i = 0; i < numProxies; ++i)
            {
                broadphase3D.Insert(static_cast<entt::entity>(i), proxies3D[i].worldAABB);
            }

            std::vector<std::pair<entt::entity, entt::entity>> candidatePairs3D;
            broadphase3D.QueryPairs(candidatePairs3D);

            for (const auto& [candA, candB] : candidatePairs3D)
            {
                auto& a = proxies3D[static_cast<size_t>(candA)];
                auto& b = proxies3D[static_cast<size_t>(candB)];

                if (!a.isDynamic && !b.isDynamic)
                    continue;

                if ((a.layer & b.mask) == 0u || (b.layer & a.mask) == 0u)
                    continue;

                HitResult hit{};
                if (a.isBox && b.isBox)
                {
                    if (!Collision::Intersects(a.worldAABB, b.worldAABB, hit))
                        continue;
                }
                else
                {
                    if (!PhysicsSystem::Overlap3D(*scene, a.entity, b.entity, hit))
                        continue;
                }

                if (hit.denetrationDepth > 0.001f)
                    anyPenetration = true;

                const vec3 oldPosA = a.transform->position;
                const vec3 oldPosB = b.transform->position;

                CollisionEvent contactEvent{};
                if (ResolvePhysicsContact3D(world, a.entity, b.entity, hit, resolveVelocity, &contactEvent))
                {
                    PhysicsCollisionPair pair{ contactEvent.entityA, contactEvent.entityB };
                    if (pair.second < pair.first)
                    {
                        std::swap(pair.first, pair.second);
                        std::swap(contactEvent.entityA, contactEvent.entityB);
                        contactEvent.contactNormal = -contactEvent.contactNormal;
                        contactEvent.relativeVelocity = -contactEvent.relativeVelocity;
                    }
                    frameCollisions3D.try_emplace(pair, contactEvent);
                }

                if (a.isDynamic)
                {
                    const vec3 deltaA = a.transform->position - oldPosA;
                    a.worldAABB.min += deltaA;
                    a.worldAABB.max += deltaA;
                }
                if (b.isDynamic)
                {
                    const vec3 deltaB = b.transform->position - oldPosB;
                    b.worldAABB.min += deltaB;
                    b.worldAABB.max += deltaB;
                }
            }

            if (iteration > 0 && !anyPenetration)
                break;
        }

        if (!TransformSystem::UpdateHierarchy(world))
        {
            if (auto* state = world.ctx().find<PhysicsTriggerState>())
                state->Events.clear();
            if (auto* colState = world.ctx().find<PhysicsCollisionState3D>())
                colState->Events.clear();

            continue;
        }

        CollectPhysicsTriggerEvents2D(*scene);
        CollectPhysicsTriggerEvents3D(*scene);
        if (const auto* state2D = world.ctx().find<PhysicsTriggerState>())
            DispatchTriggerEvents(*scene, state2D->Events);
        if (const auto* state3D = world.ctx().find<PhysicsTriggerState3D>())
            DispatchTriggerEvents(*scene, state3D->Events);

        FinalizePhysicsCollisionEvents3D(*scene, frameCollisions3D);
        if (const auto* colState3D = world.ctx().find<PhysicsCollisionState3D>())
            DispatchCollisionEvents(*scene, colState3D->Events);
    }
}
#pragma endregion

#pragma region Overlap
bool PhysicsSystem::Overlap2D(const Entity& entityA, const Entity& entityB, HitResult& outHitResult)
{
    outHitResult.Reset();

    Scene* scene = entityA.GetScene();
    if (!scene || scene != entityB.GetScene() || !scene->IsActive() || scene->IsPaused())
        return false;

    const auto handleA = entityA.GetEntityHandle();
    const auto handleB = entityB.GetEntityHandle();

    if (handleA == handleB ||
        scene->FindEntity(handleA) != &entityA ||
        scene->FindEntity(handleB) != &entityB)
        return false;

    const auto& world = scene->GetRegistry();
    if (!world.all_of<Collider2DComponent, TransformComponent, WorldTransformComponent>(handleA) ||
        !world.all_of<Collider2DComponent, TransformComponent, WorldTransformComponent>(handleB))
        return false;

    const auto& settingsA = world.get<Collider2DComponent>(handleA).GetSettings();
    const auto& settingsB = world.get<Collider2DComponent>(handleB).GetSettings();

    if ((settingsA.CollisionLayer & settingsB.CollisionMask) == 0u ||
        (settingsB.CollisionLayer & settingsA.CollisionMask) == 0u)
        return false;

    PhysicsShape2D shapeA, shapeB;
    if (!TryBuildPhysicsShape(world, handleA, shapeA) ||
        !TryBuildPhysicsShape(world, handleB, shapeB))
        return false;

    return IntersectsPhysicsShapes2D(world, handleA, handleB, shapeA, shapeB, outHitResult);
}
span<const TriggerEvent> PhysicsSystem::GetTriggerEvents2D(const Scene& scene)
{
    const auto* state = scene.GetRegistry().ctx().find<PhysicsTriggerState>();
    if (!state)
        return {};

    return { state->Events.data(), state->Events.size() };
}

void PhysicsSystem::ResetTriggerState2D(Scene& scene)
{
    scene.GetRegistry().ctx().erase<PhysicsTriggerState>();
}
#pragma endregion

#pragma region Raycast
namespace
{
    inline bool PassRaycastFilter(const entt::registry& world, entt::entity entity,
        const Engine::ColliderSettings& settings, const Engine::RaycastQuery& query)
    {
        if (entity == query.ignoreEntity) return false;
        if (!settings.Enabled) return false;
        if (!query.includeTrigger && settings.IsTrigger) return false;
        if ((settings.CollisionLayer & query.layerMask) == 0) return false;
        const auto* flag = world.try_get<Engine::FlagComponent>(entity);
        if (flag && (!flag->IsActive() || HasFlag(flag->flags, Engine::EEntityFlag::Paused) ||
            HasFlag(flag->flags, Engine::EEntityFlag::Dead)))
        {
            return false;
        }
        return true;
    }
    inline bool TestColliderHit(const Engine::Ray& ray, const Engine::ColliderComponent& collider,
        const glm::mat4& worldMatrix, float testDistance, Engine::HitResult& outHit)
    {
        const glm::mat4 colliderWorld = worldMatrix * glm::translate(glm::mat4(1.0f), collider.GetOffset());
        if (const auto* box = std::get_if<Engine::BoxShape>(&collider.GetShape()))
        {
            if (!box->HasValidParameters()) return false;
            const Engine::vec3 halfSize = box->Size * 0.5f;
            const Engine::BoundingBox b(-halfSize, halfSize);
            return Engine::Collision::Raycast(ray, b, colliderWorld, outHit, testDistance);
        }
        if (const auto* sphere = std::get_if<Engine::SphereShape>(&collider.GetShape()))
        {
            if (!sphere->HasValidParameters()) return false;
            const Engine::vec3 worldCenter = Engine::vec3(colliderWorld[3]);
            const float maxScale = glm::max(glm::max(glm::length(glm::vec3(colliderWorld[0])),
                glm::length(glm::vec3(colliderWorld[1]))), glm::length(glm::vec3(colliderWorld[2])));
            const Engine::BoundingSphere s(worldCenter, sphere->Radius * maxScale);
            return Engine::Collision::Raycast(ray, s, outHit, testDistance);
        }
        if (const auto* capsule = std::get_if<Engine::CapsuleShape>(&collider.GetShape()))
        {
            if (!capsule->HasValidParameters()) return false;
            const float halfSegment = std::max(0.0f, (capsule->Height * 0.5f) - capsule->Radius);
            const float scaleXZ = glm::max(glm::length(glm::vec3(colliderWorld[0])), glm::length(glm::vec3(colliderWorld[2])));
            const glm::vec3 pA = glm::vec3(colliderWorld * glm::vec4(0.0f, -halfSegment, 0.0f, 1.0f));
            const glm::vec3 pB = glm::vec3(colliderWorld * glm::vec4(0.0f, halfSegment, 0.0f, 1.0f));
            const Engine::Capsule cap(pA, pB, capsule->Radius * scaleXZ);
            return Engine::Collision::Raycast(ray, cap, outHit, testDistance);
        }
        if (const auto* meshShape = std::get_if<Engine::MeshShape>(&collider.GetShape()))
        {
            if (meshShape->HasMesh())
            {
                if (auto* mesh = meshShape->MeshHandle.Get())
                    return Engine::Collision::Raycast(ray, *mesh, colliderWorld, outHit, testDistance);
            }
        }
        return false;
    }
}
bool PhysicsSystem::Raycast(const Scene& scene, const Ray& ray, SceneRaycastHit& outHit, const RaycastQuery& query)
{
    if (query.maxDistance <= 0.0f || !std::isfinite(query.maxDistance)) return false;
    const float dirLenSq = glm::dot(ray.direction, ray.direction);
    if (!std::isfinite(dirLenSq) || dirLenSq <= 1.0e-6f) return false;
    const auto& world = scene.GetRegistry();
    float closestDistance = query.maxDistance;
    bool hasHit = false;
    SceneRaycastHit bestHit{};
    // 1. 3D ColliderComponent 순회 (거리 컷오프 조기 탈출)
    auto view = world.view<ColliderComponent, WorldTransformComponent>();
    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& collider = view.get<ColliderComponent>(entity);
        if (!PassRaycastFilter(world, entity, collider.GetSettings(), query)) continue;
        const auto& worldTransform = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestColliderHit(ray, collider, worldTransform.worldMatrix, closestDistance, tempHit))
        {
            if (tempHit.hasHit && tempHit.distance < closestDistance)
            {
                closestDistance = tempHit.distance;
                bestHit = { entity, tempHit };
                hasHit = true;
            }
        }
    }
    // 2. TerrainComponent 지형 순회
    auto terrainView = world.view<TerrainComponent, WorldTransformComponent>();
    for (auto entity : terrainView)
    {
        if (!IsComponentEnabled<TerrainComponent>(world, entity)) continue;
        if (entity == query.ignoreEntity) continue;
        const auto* flag = world.try_get<FlagComponent>(entity);
        if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
            HasFlag(flag->flags, EEntityFlag::Dead)))
        {
            continue;
        }
        const auto& terrain = terrainView.get<TerrainComponent>(entity);
        const auto& worldTransform = terrainView.get<WorldTransformComponent>(entity);
        HitResult terrainHit{};
        if (Collision::Raycast(ray, terrain, worldTransform.worldMatrix, terrainHit, closestDistance))
        {
            if (terrainHit.hasHit && terrainHit.distance < closestDistance)
            {
                closestDistance = terrainHit.distance;
                bestHit = { entity, terrainHit };
                hasHit = true;
            }
        }
    }
    if (hasHit)
    {
        outHit = bestHit;
        return true;
    }
    return false;
}
std::vector<SceneRaycastHit> PhysicsSystem::RaycastAll(const Scene& scene, const Ray& ray, const RaycastQuery& query)
{
    if (query.maxDistance <= 0.0f || !std::isfinite(query.maxDistance)) return {};
    const float dirLenSq = glm::dot(ray.direction, ray.direction);
    if (!std::isfinite(dirLenSq) || dirLenSq <= 1.0e-6f) return {};
    std::vector<SceneRaycastHit> results;
    const auto& world = scene.GetRegistry();
    // 1. 3D ColliderComponent 순회
    auto view = world.view<ColliderComponent, WorldTransformComponent>();
    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& collider = view.get<ColliderComponent>(entity);
        if (!PassRaycastFilter(world, entity, collider.GetSettings(), query)) continue;
        const auto& worldTransform = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestColliderHit(ray, collider, worldTransform.worldMatrix, query.maxDistance, tempHit))
        {
            if (tempHit.hasHit && tempHit.distance <= query.maxDistance)
                results.push_back({ entity, tempHit });
        }
    }
    // 2. TerrainComponent 지형 순회
    auto terrainView = world.view<TerrainComponent, WorldTransformComponent>();
    for (auto entity : terrainView)
    {
        if (!IsComponentEnabled<TerrainComponent>(world, entity)) continue;
        if (entity == query.ignoreEntity) continue;
        const auto* flag = world.try_get<FlagComponent>(entity);
        if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
            HasFlag(flag->flags, EEntityFlag::Dead)))
        {
            continue;
        }
        const auto& terrain = terrainView.get<TerrainComponent>(entity);
        const auto& worldTransform = terrainView.get<WorldTransformComponent>(entity);
        HitResult terrainHit{};
        if (Collision::Raycast(ray, terrain, worldTransform.worldMatrix, terrainHit, query.maxDistance))
        {
            if (terrainHit.hasHit && terrainHit.distance <= query.maxDistance)
                results.push_back({ entity, terrainHit });
        }
    }
    // 3. 가장 가까운 거리 순으로 정렬
    std::sort(results.begin(), results.end(), [](const SceneRaycastHit& a, const SceneRaycastHit& b) {
        return a.Hit.distance < b.Hit.distance;
        });
    return results;
}
#pragma endregion

// ── 3. 3D Overlap 및 트리거 수집/디스패치 구현부 ──
static bool TryGetWorldCapsuleFromCollider(const ColliderComponent& col, const mat4& worldMatrix, Capsule& outCapsule)
{
    const auto* capShape = std::get_if<CapsuleShape>(&col.GetShape());
    if (!capShape || !capShape->HasValidParameters()) return false;
    const mat4 colliderWorld = worldMatrix * glm::translate(mat4(1.0f), col.GetOffset());
    const float halfSegment = std::max(0.0f, (capShape->Height * 0.5f) - capShape->Radius);
    const float scaleXZ = glm::max(glm::length(vec3(colliderWorld[0])), glm::length(vec3(colliderWorld[2])));
    const vec3 pA = vec3(colliderWorld * vec4(0.0f, -halfSegment, 0.0f, 1.0f));
    const vec3 pB = vec3(colliderWorld * vec4(0.0f, halfSegment, 0.0f, 1.0f));
    outCapsule = Capsule(pA, pB, capShape->Radius * scaleXZ);
    return true;
}

bool PhysicsSystem::Overlap3D(const Scene& scene, entt::entity entityA, entt::entity entityB, HitResult& outHitResult)
{
    outHitResult.Reset();
    const auto& world = scene.GetRegistry();
    if (!world.all_of<WorldTransformComponent>(entityA) || !world.all_of<WorldTransformComponent>(entityB))
        return false;

    const auto& trA = world.get<WorldTransformComponent>(entityA);
    const auto& trB = world.get<WorldTransformComponent>(entityB);

    const auto* kccA = TryGetEnabledComponent<CharacterControllerComponent>(world, entityA);
    const auto* kccB = TryGetEnabledComponent<CharacterControllerComponent>(world, entityB);
    const auto* colA = TryGetEnabledComponent<ColliderComponent>(world, entityA);
    const auto* colB = TryGetEnabledComponent<ColliderComponent>(world, entityB);

    // KCC vs KCC
    if (kccA && kccB)
    {
        const Capsule capA = kccA->GetWorldCapsule(vec3(trA.worldMatrix[3]));
        const Capsule capB = kccB->GetWorldCapsule(vec3(trB.worldMatrix[3]));
        return Collision::Intersects3D(capA, capB, outHitResult);
    }

    // KCC vs Collider
    if (kccA && colB)
    {
        const Capsule capA = kccA->GetWorldCapsule(vec3(trA.worldMatrix[3]));
        const mat4 worldB = trB.worldMatrix * glm::translate(mat4(1.0f), colB->GetOffset());
        if (const auto* sphere = std::get_if<SphereShape>(&colB->GetShape()))
        {
            const f32 scale = glm::max(glm::max(glm::length(vec3(worldB[0])), glm::length(vec3(worldB[1]))), glm::length(vec3(worldB[2])));
            return Collision::Intersects(capA, BoundingSphere(vec3(worldB[3]), sphere->Radius * scale), outHitResult);
        }
        if (const auto* box = std::get_if<BoxShape>(&colB->GetShape()))
        {
            const BoundingBox b(-box->Size * 0.5f, box->Size * 0.5f);
            return Collision::Intersects(capA, b, worldB, outHitResult);
        }
        if (std::holds_alternative<CapsuleShape>(colB->GetShape()))
        {
            Capsule capB;
            if (TryGetWorldCapsuleFromCollider(*colB, trB.worldMatrix, capB))
                return Collision::Intersects3D(capA, capB, outHitResult);
        }
        return false;
    }
    if (colA && kccB)
    {
        const bool hit = Overlap3D(scene, entityB, entityA, outHitResult);
        if (hit) outHitResult.normal = -outHitResult.normal;
        return hit;
    }

    // Collider vs Collider
    if (colA && colB)
    {
        const mat4 worldA = trA.worldMatrix * glm::translate(mat4(1.0f), colA->GetOffset());
        const mat4 worldB = trB.worldMatrix * glm::translate(mat4(1.0f), colB->GetOffset());
        const auto* sphereA = std::get_if<SphereShape>(&colA->GetShape());
        const auto* sphereB = std::get_if<SphereShape>(&colB->GetShape());
        const auto* boxA = std::get_if<BoxShape>(&colA->GetShape());
        const auto* boxB = std::get_if<BoxShape>(&colB->GetShape());
        const auto* capA = std::get_if<CapsuleShape>(&colA->GetShape());
        const auto* capB = std::get_if<CapsuleShape>(&colB->GetShape());

        if (sphereA && sphereB)
        {
            const f32 scaleA = glm::max(glm::max(glm::length(vec3(worldA[0])), glm::length(vec3(worldA[1]))), glm::length(vec3(worldA[2])));
            const f32 scaleB = glm::max(glm::max(glm::length(vec3(worldB[0])), glm::length(vec3(worldB[1]))), glm::length(vec3(worldB[2])));
            return Collision::Intersects(BoundingSphere(vec3(worldA[3]), sphereA->Radius * scaleA),
                                         BoundingSphere(vec3(worldB[3]), sphereB->Radius * scaleB), outHitResult);
        }
        if (boxA && boxB)
        {
            return Collision::Intersects(BoundingBox(-boxA->Size * 0.5f, boxA->Size * 0.5f), worldA,
                                         BoundingBox(-boxB->Size * 0.5f, boxB->Size * 0.5f), worldB, outHitResult);
        }
        if (boxA && sphereB)
        {
            const f32 scaleB = glm::max(glm::max(glm::length(vec3(worldB[0])), glm::length(vec3(worldB[1]))), glm::length(vec3(worldB[2])));
            const f32 sx = glm::length(vec3(worldA[0]));
            const f32 sy = glm::length(vec3(worldA[1]));
            const f32 sz = glm::length(vec3(worldA[2]));
            if (sx <= 1.0e-6f || sy <= 1.0e-6f || sz <= 1.0e-6f)
                return false;

            const vec3 axisX = vec3(worldA[0]) / sx;
            const vec3 axisY = vec3(worldA[1]) / sy;
            const vec3 axisZ = vec3(worldA[2]) / sz;
            const vec3 boxCenter = vec3(worldA[3]);
            const vec3 halfExtents = (boxA->Size * 0.5f) * vec3(sx, sy, sz);

            const vec3 rel = vec3(worldB[3]) - boxCenter;
            const vec3 localSphereCenter(glm::dot(rel, axisX), glm::dot(rel, axisY), glm::dot(rel, axisZ));
            const f32 worldRadius = sphereB->Radius * scaleB;

            HitResult localHit{};
            if (!Collision::Intersects(BoundingBox(-halfExtents, halfExtents), BoundingSphere(localSphereCenter, worldRadius), localHit))
                return false;

            outHitResult.point = boxCenter + axisX * localHit.point.x + axisY * localHit.point.y + axisZ * localHit.point.z;
            outHitResult.normal = glm::normalize(axisX * localHit.normal.x + axisY * localHit.normal.y + axisZ * localHit.normal.z);
            outHitResult.denetrationDepth = localHit.denetrationDepth;
            outHitResult.hasHit = true;
            return true;
        }
        if (sphereA && boxB)
        {
            const bool hit = Overlap3D(scene, entityB, entityA, outHitResult);
            if (hit) outHitResult.normal = -outHitResult.normal;
            return hit;
        }
        if (capA && capB)
        {
            Capsule cA, cB;
            if (TryGetWorldCapsuleFromCollider(*colA, trA.worldMatrix, cA) && TryGetWorldCapsuleFromCollider(*colB, trB.worldMatrix, cB))
                return Collision::Intersects3D(cA, cB, outHitResult);
        }
        if (capA && sphereB)
        {
            Capsule cA;
            if (TryGetWorldCapsuleFromCollider(*colA, trA.worldMatrix, cA))
            {
                const f32 scaleB = glm::max(glm::max(glm::length(vec3(worldB[0])), glm::length(vec3(worldB[1]))), glm::length(vec3(worldB[2])));
                return Collision::Intersects(cA, BoundingSphere(vec3(worldB[3]), sphereB->Radius * scaleB), outHitResult);
            }
        }
        if (sphereA && capB)
        {
            const bool hit = Overlap3D(scene, entityB, entityA, outHitResult);
            if (hit) outHitResult.normal = -outHitResult.normal;
            return hit;
        }
        if (capA && boxB)
        {
            Capsule cA;
            if (TryGetWorldCapsuleFromCollider(*colA, trA.worldMatrix, cA))
                return Collision::Intersects(cA, BoundingBox(-boxB->Size * 0.5f, boxB->Size * 0.5f), worldB, outHitResult);
        }
        if (boxA && capB)
        {
            const bool hit = Overlap3D(scene, entityB, entityA, outHitResult);
            if (hit) outHitResult.normal = -outHitResult.normal;
            return hit;
        }
    }
    return false;
}
void PhysicsSystem::DispatchTriggerEvents(Scene& scene, const std::vector<TriggerEvent>& events)
{
    auto& world = scene.GetRegistry();
    for (const auto& event : events)
    {
        const auto invoke = [&](entt::entity self, entt::entity other) {
            if (!world.valid(self)) return;
            // 1. C++ TriggerCallbackComponent 델리게이트 호출
            if (auto* cb = TryGetEnabledComponent<TriggerCallbackComponent>(world, self))
            {
                if (event.type == ETriggerEventType::Enter && cb->OnEnter) cb->OnEnter(self, other, event.type);
                else if (event.type == ETriggerEventType::Stay && cb->OnStay) cb->OnStay(self, other, event.type);
                else if (event.type == ETriggerEventType::Exit && cb->OnExit) cb->OnExit(self, other, event.type);
            }
            // 2. 유니티 스타일 NativeScriptComponent 가상 함수 호출
            if (auto* ns = TryGetEnabledComponent<NativeScriptComponent>(world, self))
            {
                if (!ns->Instance && ns->InstantiateScript)
                {
                    ns->Instance = ns->InstantiateScript();
                    ns->Instance->m_Entity = self;
                    ns->Instance->m_Scene = &scene;
                    ns->Instance->OnCreate();
                }
                if (ns->Instance)
                {
                    if (event.type == ETriggerEventType::Enter) ns->Instance->OnTriggerEnter(other);
                    else if (event.type == ETriggerEventType::Stay) ns->Instance->OnTriggerStay(other);
                    else if (event.type == ETriggerEventType::Exit) ns->Instance->OnTriggerExit(other);
                }
            }
            // 3. 비주얼 스크립팅 GraphBindingComponent 이벤트 노드 디스패치
            if (world.all_of<GraphBindingComponent>(self))
            {
                GraphSystem::DispatchTriggerEvent(&scene, self, other, static_cast<uint8>(event.type));
            }
            };
        invoke(event.entityA, event.entityB);
        invoke(event.entityB, event.entityA);
        // 3. 전역 리스너 브로드캐스트
        for (const auto& callback : s_GlobalTriggerCallbacks)
        {
            if (callback) callback(event);
        }
    }
}
std::span<const TriggerEvent> PhysicsSystem::GetTriggerEvents3D(const Scene& scene)
{
    const auto* state = scene.GetRegistry().ctx().find<PhysicsTriggerState3D>();
    if (!state) return {};
    return { state->Events.data(), state->Events.size() };
}
void PhysicsSystem::ResetTriggerState3D(Scene& scene)
{
    scene.GetRegistry().ctx().erase<PhysicsTriggerState3D>();
}
void PhysicsSystem::RegisterGlobalTriggerCallback(GlobalTriggerCallback callback)
{
    s_GlobalTriggerCallbacks.push_back(std::move(callback));
}
void PhysicsSystem::ClearGlobalTriggerCallbacks()
{
    s_GlobalTriggerCallbacks.clear();
}

void PhysicsSystem::DispatchCollisionEvents(Scene& scene, const std::vector<CollisionEvent>& events)
{
    auto& world = scene.GetRegistry();
    for (const auto& event : events)
    {
        const auto invoke = [&](entt::entity self, entt::entity other)
        {
            if (!world.valid(self)) return;

            if (auto* cb = TryGetEnabledComponent<CollisionCallbackComponent>(world, self))
            {
                if (event.type == ECollisionEventType::Enter && cb->OnCollisionEnter)
                    cb->OnCollisionEnter(self, other, event);
                else if (event.type == ECollisionEventType::Stay && cb->OnCollisionStay)
                    cb->OnCollisionStay(self, other, event);
                else if (event.type == ECollisionEventType::Exit && cb->OnCollisionExit)
                    cb->OnCollisionExit(self, other, event);
            }
        };

        invoke(event.entityA, event.entityB);
        invoke(event.entityB, event.entityA);

        for (const auto& callback : s_GlobalCollisionCallbacks)
        {
            if (callback) callback(event);
        }
    }
}

std::span<const CollisionEvent> PhysicsSystem::GetCollisionEvents3D(const Scene& scene)
{
    const auto* state = scene.GetRegistry().ctx().find<PhysicsCollisionState3D>();
    if (!state) return {};
    return { state->Events.data(), state->Events.size() };
}

void PhysicsSystem::ResetCollisionState3D(Scene& scene)
{
    scene.GetRegistry().ctx().erase<PhysicsCollisionState3D>();
}

void PhysicsSystem::RegisterGlobalCollisionCallback(GlobalCollisionCallback callback)
{
    s_GlobalCollisionCallbacks.push_back(std::move(callback));
}

void PhysicsSystem::ClearGlobalCollisionCallbacks()
{
    s_GlobalCollisionCallbacks.clear();
}

#pragma region Sweep & Overlap
static bool IsMatrixAxisAligned(const mat4& matrix)
{
    for (int i = 0; i < 3; ++i)
    {
        vec3 offAxis(matrix[i]);
        const f32 scale = std::abs(offAxis[i]);
        offAxis[i] = 0.0f;
        if (glm::dot(offAxis, offAxis) > scale * scale * 1.0e-10f) return false;
    }
    return true;
}

static bool PassSweepFilter(const entt::registry& world, entt::entity entity, const ColliderSettings& settings, const SweepQuery& query)
{
    if (entity == query.ignoreEntity) return false;
    if (!settings.Enabled) return false;
    if (settings.IsTrigger && !query.includeTrigger) return false;
    if ((settings.CollisionLayer & query.layerMask) == 0u) return false;
    const auto* flag = world.try_get<FlagComponent>(entity);
    if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
        HasFlag(flag->flags, EEntityFlag::Dead)))
        return false;
    return true;
}

static bool PassOverlapFilter(const entt::registry& world, entt::entity entity, const ColliderSettings& settings, const OverlapQuery& query)
{
    if (entity == query.ignoreEntity) return false;
    if (!settings.Enabled) return false;
    if (settings.IsTrigger && !query.includeTrigger) return false;
    if ((settings.CollisionLayer & query.layerMask) == 0u) return false;
    const auto* flag = world.try_get<FlagComponent>(entity);
    if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
        HasFlag(flag->flags, EEntityFlag::Dead)))
        return false;
    return true;
}

static bool TestSweepColliderSphere(const BoundingSphere& sphere, const vec3& dir, f32 maxDist,
    const ColliderComponent& col, const mat4& worldMatrix, HitResult& outHit)
{
    const mat4 colliderWorld = worldMatrix * glm::translate(mat4(1.0f), col.GetOffset());
    if (const auto* box = std::get_if<BoxShape>(&col.GetShape()))
    {
        if (!box->HasValidParameters()) return false;
        const vec3 halfSize = box->Size * 0.5f;
        if (!IsMatrixAxisAligned(colliderWorld))
        {
            const Capsule cap(sphere.center, sphere.center, sphere.radius);
            const BoundingBox localBox(-halfSize, halfSize);
            return Collision::Sweep(cap, dir, maxDist, localBox, colliderWorld, outHit);
        }
        else
        {
            const vec3 worldCenter = vec3(colliderWorld[3]);
            const vec3 extent = glm::abs(vec3(colliderWorld[0])) * halfSize.x +
                                glm::abs(vec3(colliderWorld[1])) * halfSize.y +
                                glm::abs(vec3(colliderWorld[2])) * halfSize.z;
            const BoundingBox obstacle(worldCenter - extent, worldCenter + extent);
            return Collision::Sweep(sphere, dir, maxDist, obstacle, outHit);
        }
    }
    if (const auto* sph = std::get_if<SphereShape>(&col.GetShape()))
    {
        if (!sph->HasValidParameters()) return false;
        const f32 scale = glm::max(glm::length(vec3(colliderWorld[0])), glm::max(glm::length(vec3(colliderWorld[1])), glm::length(vec3(colliderWorld[2]))));
        const BoundingSphere obstacle(vec3(colliderWorld[3]), sph->Radius * scale);
        return Collision::Sweep(sphere, dir, maxDist, obstacle, outHit);
    }
    if (const auto* cap = std::get_if<CapsuleShape>(&col.GetShape()))
    {
        if (!cap->HasValidParameters()) return false;
        const f32 halfSegment = std::max(0.0f, (cap->Height * 0.5f) - cap->Radius);
        const f32 scaleXZ = glm::max(glm::length(vec3(colliderWorld[0])), glm::length(vec3(colliderWorld[2])));
        const vec3 pA = vec3(colliderWorld * vec4(0.0f, -halfSegment, 0.0f, 1.0f));
        const vec3 pB = vec3(colliderWorld * vec4(0.0f, halfSegment, 0.0f, 1.0f));
        const Capsule obstacleCap(pA, pB, cap->Radius * scaleXZ);
        const Capsule sphereAsCap(sphere.center, sphere.center, sphere.radius);
        return Collision::Sweep(sphereAsCap, dir, maxDist, obstacleCap, outHit);
    }
    return false;
}

static bool TestSweepColliderBox(const BoundingBox& box, const vec3& dir, f32 maxDist,
    const ColliderComponent& col, const mat4& worldMatrix, HitResult& outHit)
{
    const mat4 colliderWorld = worldMatrix * glm::translate(mat4(1.0f), col.GetOffset());
    if (const auto* b = std::get_if<BoxShape>(&col.GetShape()))
    {
        if (!b->HasValidParameters()) return false;
        const vec3 halfSize = b->Size * 0.5f;
        const vec3 worldCenter = vec3(colliderWorld[3]);
        const vec3 extent = glm::abs(vec3(colliderWorld[0])) * halfSize.x +
                            glm::abs(vec3(colliderWorld[1])) * halfSize.y +
                            glm::abs(vec3(colliderWorld[2])) * halfSize.z;
        const BoundingBox obstacle(worldCenter - extent, worldCenter + extent);
        return Collision::Sweep(box, dir, maxDist, obstacle, outHit);
    }
    if (const auto* sph = std::get_if<SphereShape>(&col.GetShape()))
    {
        if (!sph->HasValidParameters()) return false;
        const f32 scale = glm::max(glm::length(vec3(colliderWorld[0])), glm::max(glm::length(vec3(colliderWorld[1])), glm::length(vec3(colliderWorld[2]))));
        const BoundingSphere obstacle(vec3(colliderWorld[3]), sph->Radius * scale);
        return Collision::Sweep(box, dir, maxDist, obstacle, outHit);
    }
    if (const auto* cap = std::get_if<CapsuleShape>(&col.GetShape()))
    {
        if (!cap->HasValidParameters()) return false;
        BoundingBox capAABB{};
        if (!ComputeColliderWorldAABB3D(col, worldMatrix, capAABB)) return false;
        return Collision::Sweep(box, dir, maxDist, capAABB, outHit);
    }
    return false;
}

static bool TestSweepColliderCapsule(const Capsule& capsule, const vec3& dir, f32 maxDist,
    const ColliderComponent& col, const mat4& worldMatrix, HitResult& outHit)
{
    const mat4 colliderWorld = worldMatrix * glm::translate(mat4(1.0f), col.GetOffset());
    if (const auto* box = std::get_if<BoxShape>(&col.GetShape()))
    {
        if (!box->HasValidParameters()) return false;
        const vec3 halfSize = box->Size * 0.5f;
        if (!IsMatrixAxisAligned(colliderWorld))
        {
            const BoundingBox localBox(-halfSize, halfSize);
            return Collision::Sweep(capsule, dir, maxDist, localBox, colliderWorld, outHit);
        }
        else
        {
            const vec3 worldCenter = vec3(colliderWorld[3]);
            const vec3 extent = glm::abs(vec3(colliderWorld[0])) * halfSize.x +
                                glm::abs(vec3(colliderWorld[1])) * halfSize.y +
                                glm::abs(vec3(colliderWorld[2])) * halfSize.z;
            const BoundingBox obstacle(worldCenter - extent, worldCenter + extent);
            return Collision::Sweep(capsule, dir, maxDist, obstacle, outHit);
        }
    }
    if (const auto* sph = std::get_if<SphereShape>(&col.GetShape()))
    {
        if (!sph->HasValidParameters()) return false;
        const f32 scale = glm::max(glm::length(vec3(colliderWorld[0])), glm::max(glm::length(vec3(colliderWorld[1])), glm::length(vec3(colliderWorld[2]))));
        const BoundingSphere obstacle(vec3(colliderWorld[3]), sph->Radius * scale);
        return Collision::Sweep(capsule, dir, maxDist, obstacle, outHit);
    }
    if (const auto* cap = std::get_if<CapsuleShape>(&col.GetShape()))
    {
        if (!cap->HasValidParameters()) return false;
        const f32 halfSegment = std::max(0.0f, (cap->Height * 0.5f) - cap->Radius);
        const f32 scaleXZ = glm::max(glm::length(vec3(colliderWorld[0])), glm::length(vec3(colliderWorld[2])));
        const vec3 pA = vec3(colliderWorld * vec4(0.0f, -halfSegment, 0.0f, 1.0f));
        const vec3 pB = vec3(colliderWorld * vec4(0.0f, halfSegment, 0.0f, 1.0f));
        const Capsule obstacleCap(pA, pB, cap->Radius * scaleXZ);
        return Collision::Sweep(capsule, dir, maxDist, obstacleCap, outHit);
    }
    return false;
}

static bool TestOverlapColliderSphere(const BoundingSphere& sphere, const ColliderComponent& col, const mat4& worldMatrix, HitResult& outHit)
{
    const mat4 colliderWorld = worldMatrix * glm::translate(mat4(1.0f), col.GetOffset());
    if (const auto* box = std::get_if<BoxShape>(&col.GetShape()))
    {
        if (!box->HasValidParameters()) return false;
        const vec3 halfSize = box->Size * 0.5f;
        if (!IsMatrixAxisAligned(colliderWorld))
        {
            const BoundingBox localBox(-halfSize, halfSize);
            const Capsule sphereAsCapsule(sphere.center, sphere.center, sphere.radius);
            return Collision::Intersects(sphereAsCapsule, localBox, colliderWorld, outHit);
        }
        else
        {
            const vec3 worldCenter = vec3(colliderWorld[3]);
            const vec3 extent = glm::abs(vec3(colliderWorld[0])) * halfSize.x +
                                glm::abs(vec3(colliderWorld[1])) * halfSize.y +
                                glm::abs(vec3(colliderWorld[2])) * halfSize.z;
            const BoundingBox obstacle(worldCenter - extent, worldCenter + extent);
            return Collision::Intersects(obstacle, sphere, outHit);
        }
    }
    if (const auto* sph = std::get_if<SphereShape>(&col.GetShape()))
    {
        if (!sph->HasValidParameters()) return false;
        const f32 scale = glm::max(glm::length(vec3(colliderWorld[0])), glm::max(glm::length(vec3(colliderWorld[1])), glm::length(vec3(colliderWorld[2]))));
        const BoundingSphere obstacle(vec3(colliderWorld[3]), sph->Radius * scale);
        return Collision::Intersects(sphere, obstacle, outHit);
    }
    if (const auto* cap = std::get_if<CapsuleShape>(&col.GetShape()))
    {
        if (!cap->HasValidParameters()) return false;
        const f32 halfSegment = std::max(0.0f, (cap->Height * 0.5f) - cap->Radius);
        const f32 scaleXZ = glm::max(glm::length(vec3(colliderWorld[0])), glm::length(vec3(colliderWorld[2])));
        const vec3 pA = vec3(colliderWorld * vec4(0.0f, -halfSegment, 0.0f, 1.0f));
        const vec3 pB = vec3(colliderWorld * vec4(0.0f, halfSegment, 0.0f, 1.0f));
        const Capsule obstacleCap(pA, pB, cap->Radius * scaleXZ);
        return Collision::Intersects(obstacleCap, sphere, outHit);
    }
    return false;
}

static bool TestOverlapColliderBox(const BoundingBox& box, const ColliderComponent& col, const mat4& worldMatrix, HitResult& outHit)
{
    const mat4 colliderWorld = worldMatrix * glm::translate(mat4(1.0f), col.GetOffset());
    if (const auto* b = std::get_if<BoxShape>(&col.GetShape()))
    {
        if (!b->HasValidParameters()) return false;
        const vec3 halfSize = b->Size * 0.5f;
        const vec3 worldCenter = vec3(colliderWorld[3]);
        const vec3 extent = glm::abs(vec3(colliderWorld[0])) * halfSize.x +
                            glm::abs(vec3(colliderWorld[1])) * halfSize.y +
                            glm::abs(vec3(colliderWorld[2])) * halfSize.z;
        const BoundingBox obstacle(worldCenter - extent, worldCenter + extent);
        return Collision::Intersects(box, obstacle, outHit);
    }
    if (const auto* sph = std::get_if<SphereShape>(&col.GetShape()))
    {
        if (!sph->HasValidParameters()) return false;
        const f32 scale = glm::max(glm::length(vec3(colliderWorld[0])), glm::max(glm::length(vec3(colliderWorld[1])), glm::length(vec3(colliderWorld[2]))));
        const BoundingSphere obstacle(vec3(colliderWorld[3]), sph->Radius * scale);
        return Collision::Intersects(box, obstacle, outHit);
    }
    if (const auto* cap = std::get_if<CapsuleShape>(&col.GetShape()))
    {
        if (!cap->HasValidParameters()) return false;
        const f32 halfSegment = std::max(0.0f, (cap->Height * 0.5f) - cap->Radius);
        const f32 scaleXZ = glm::max(glm::length(vec3(colliderWorld[0])), glm::length(vec3(colliderWorld[2])));
        const vec3 pA = vec3(colliderWorld * vec4(0.0f, -halfSegment, 0.0f, 1.0f));
        const vec3 pB = vec3(colliderWorld * vec4(0.0f, halfSegment, 0.0f, 1.0f));
        const Capsule obstacleCap(pA, pB, cap->Radius * scaleXZ);
        return Collision::Intersects(obstacleCap, box, outHit);
    }
    return false;
}

static bool TestOverlapColliderCapsule(const Capsule& capsule, const ColliderComponent& col, const mat4& worldMatrix, HitResult& outHit)
{
    const mat4 colliderWorld = worldMatrix * glm::translate(mat4(1.0f), col.GetOffset());
    if (const auto* box = std::get_if<BoxShape>(&col.GetShape()))
    {
        if (!box->HasValidParameters()) return false;
        const vec3 halfSize = box->Size * 0.5f;
        if (!IsMatrixAxisAligned(colliderWorld))
        {
            const BoundingBox localBox(-halfSize, halfSize);
            return Collision::Intersects(capsule, localBox, colliderWorld, outHit);
        }
        else
        {
            const vec3 worldCenter = vec3(colliderWorld[3]);
            const vec3 extent = glm::abs(vec3(colliderWorld[0])) * halfSize.x +
                                glm::abs(vec3(colliderWorld[1])) * halfSize.y +
                                glm::abs(vec3(colliderWorld[2])) * halfSize.z;
            const BoundingBox obstacle(worldCenter - extent, worldCenter + extent);
            return Collision::Intersects(capsule, obstacle, outHit);
        }
    }
    if (const auto* sph = std::get_if<SphereShape>(&col.GetShape()))
    {
        if (!sph->HasValidParameters()) return false;
        const f32 scale = glm::max(glm::length(vec3(colliderWorld[0])), glm::max(glm::length(vec3(colliderWorld[1])), glm::length(vec3(colliderWorld[2]))));
        const BoundingSphere obstacle(vec3(colliderWorld[3]), sph->Radius * scale);
        return Collision::Intersects(capsule, obstacle, outHit);
    }
    if (const auto* cap = std::get_if<CapsuleShape>(&col.GetShape()))
    {
        if (!cap->HasValidParameters()) return false;
        const f32 halfSegment = std::max(0.0f, (cap->Height * 0.5f) - cap->Radius);
        const f32 scaleXZ = glm::max(glm::length(vec3(colliderWorld[0])), glm::length(vec3(colliderWorld[2])));
        const vec3 pA = vec3(colliderWorld * vec4(0.0f, -halfSegment, 0.0f, 1.0f));
        const vec3 pB = vec3(colliderWorld * vec4(0.0f, halfSegment, 0.0f, 1.0f));
        const Capsule obstacleCap(pA, pB, cap->Radius * scaleXZ);
        return Collision::Intersects3D(capsule, obstacleCap, outHit);
    }
    return false;
}

bool PhysicsSystem::Sweep(const Scene& scene, const BoundingSphere& sphere, const vec3& direction, SceneSweepHit& outHit, const SweepQuery& query)
{
    outHit = {};
    if (query.maxDistance <= 0.0f || !std::isfinite(query.maxDistance)) return false;
    const f32 dirLenSq = glm::dot(direction, direction);
    if (!std::isfinite(dirLenSq) || dirLenSq <= 1.0e-6f) return false;
    const vec3 dir = glm::normalize(direction);

    const auto& world = scene.GetRegistry();
    auto view = world.view<ColliderComponent, WorldTransformComponent>();

    f32 closestDistance = query.maxDistance;
    bool hasHit = false;
    SceneSweepHit bestHit{};

    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = view.get<ColliderComponent>(entity);
        if (!PassSweepFilter(world, entity, col.GetSettings(), query)) continue;

        const auto& worldTr = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestSweepColliderSphere(sphere, dir, closestDistance, col, worldTr.worldMatrix, tempHit))
        {
            if (tempHit.hasHit && tempHit.distance < closestDistance)
            {
                closestDistance = tempHit.distance;
                bestHit = { entity, tempHit };
                hasHit = true;
            }
        }
    }

    if (hasHit)
    {
        outHit = bestHit;
        return true;
    }
    return false;
}

bool PhysicsSystem::Sweep(const Scene& scene, const BoundingBox& box, const vec3& direction, SceneSweepHit& outHit, const SweepQuery& query)
{
    outHit = {};
    if (query.maxDistance <= 0.0f || !std::isfinite(query.maxDistance)) return false;
    const f32 dirLenSq = glm::dot(direction, direction);
    if (!std::isfinite(dirLenSq) || dirLenSq <= 1.0e-6f) return false;
    const vec3 dir = glm::normalize(direction);

    const auto& world = scene.GetRegistry();
    auto view = world.view<ColliderComponent, WorldTransformComponent>();

    f32 closestDistance = query.maxDistance;
    bool hasHit = false;
    SceneSweepHit bestHit{};

    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = view.get<ColliderComponent>(entity);
        if (!PassSweepFilter(world, entity, col.GetSettings(), query)) continue;

        const auto& worldTr = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestSweepColliderBox(box, dir, closestDistance, col, worldTr.worldMatrix, tempHit))
        {
            if (tempHit.hasHit && tempHit.distance < closestDistance)
            {
                closestDistance = tempHit.distance;
                bestHit = { entity, tempHit };
                hasHit = true;
            }
        }
    }

    if (hasHit)
    {
        outHit = bestHit;
        return true;
    }
    return false;
}

bool PhysicsSystem::Sweep(const Scene& scene, const Capsule& capsule, const vec3& direction, SceneSweepHit& outHit, const SweepQuery& query)
{
    outHit = {};
    if (query.maxDistance <= 0.0f || !std::isfinite(query.maxDistance)) return false;
    const f32 dirLenSq = glm::dot(direction, direction);
    if (!std::isfinite(dirLenSq) || dirLenSq <= 1.0e-6f) return false;
    const vec3 dir = glm::normalize(direction);

    const auto& world = scene.GetRegistry();
    auto view = world.view<ColliderComponent, WorldTransformComponent>();

    f32 closestDistance = query.maxDistance;
    bool hasHit = false;
    SceneSweepHit bestHit{};

    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = view.get<ColliderComponent>(entity);
        if (!PassSweepFilter(world, entity, col.GetSettings(), query)) continue;

        const auto& worldTr = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestSweepColliderCapsule(capsule, dir, closestDistance, col, worldTr.worldMatrix, tempHit))
        {
            if (tempHit.hasHit && tempHit.distance < closestDistance)
            {
                closestDistance = tempHit.distance;
                bestHit = { entity, tempHit };
                hasHit = true;
            }
        }
    }

    if (hasHit)
    {
        outHit = bestHit;
        return true;
    }
    return false;
}

std::vector<SceneSweepHit> PhysicsSystem::SweepAll(const Scene& scene, const BoundingSphere& sphere, const vec3& direction, const SweepQuery& query)
{
    if (query.maxDistance <= 0.0f || !std::isfinite(query.maxDistance)) return {};
    const f32 dirLenSq = glm::dot(direction, direction);
    if (!std::isfinite(dirLenSq) || dirLenSq <= 1.0e-6f) return {};
    const vec3 dir = glm::normalize(direction);

    std::vector<SceneSweepHit> results;
    const auto& world = scene.GetRegistry();
    auto view = world.view<ColliderComponent, WorldTransformComponent>();

    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = view.get<ColliderComponent>(entity);
        if (!PassSweepFilter(world, entity, col.GetSettings(), query)) continue;

        const auto& worldTr = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestSweepColliderSphere(sphere, dir, query.maxDistance, col, worldTr.worldMatrix, tempHit))
        {
            if (tempHit.hasHit && tempHit.distance <= query.maxDistance)
                results.push_back({ entity, tempHit });
        }
    }

    std::sort(results.begin(), results.end(), [](const SceneSweepHit& a, const SceneSweepHit& b) {
        return a.Hit.distance < b.Hit.distance;
    });
    return results;
}

std::vector<SceneSweepHit> PhysicsSystem::SweepAll(const Scene& scene, const BoundingBox& box, const vec3& direction, const SweepQuery& query)
{
    if (query.maxDistance <= 0.0f || !std::isfinite(query.maxDistance)) return {};
    const f32 dirLenSq = glm::dot(direction, direction);
    if (!std::isfinite(dirLenSq) || dirLenSq <= 1.0e-6f) return {};
    const vec3 dir = glm::normalize(direction);

    std::vector<SceneSweepHit> results;
    const auto& world = scene.GetRegistry();
    auto view = world.view<ColliderComponent, WorldTransformComponent>();

    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = view.get<ColliderComponent>(entity);
        if (!PassSweepFilter(world, entity, col.GetSettings(), query)) continue;

        const auto& worldTr = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestSweepColliderBox(box, dir, query.maxDistance, col, worldTr.worldMatrix, tempHit))
        {
            if (tempHit.hasHit && tempHit.distance <= query.maxDistance)
                results.push_back({ entity, tempHit });
        }
    }

    std::sort(results.begin(), results.end(), [](const SceneSweepHit& a, const SceneSweepHit& b) {
        return a.Hit.distance < b.Hit.distance;
    });
    return results;
}

std::vector<SceneSweepHit> PhysicsSystem::SweepAll(const Scene& scene, const Capsule& capsule, const vec3& direction, const SweepQuery& query)
{
    if (query.maxDistance <= 0.0f || !std::isfinite(query.maxDistance)) return {};
    const f32 dirLenSq = glm::dot(direction, direction);
    if (!std::isfinite(dirLenSq) || dirLenSq <= 1.0e-6f) return {};
    const vec3 dir = glm::normalize(direction);

    std::vector<SceneSweepHit> results;
    const auto& world = scene.GetRegistry();
    auto view = world.view<ColliderComponent, WorldTransformComponent>();

    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = view.get<ColliderComponent>(entity);
        if (!PassSweepFilter(world, entity, col.GetSettings(), query)) continue;

        const auto& worldTr = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestSweepColliderCapsule(capsule, dir, query.maxDistance, col, worldTr.worldMatrix, tempHit))
        {
            if (tempHit.hasHit && tempHit.distance <= query.maxDistance)
                results.push_back({ entity, tempHit });
        }
    }

    std::sort(results.begin(), results.end(), [](const SceneSweepHit& a, const SceneSweepHit& b) {
        return a.Hit.distance < b.Hit.distance;
    });
    return results;
}

std::vector<SceneOverlapHit> PhysicsSystem::OverlapAll(const Scene& scene, const BoundingSphere& sphere, const OverlapQuery& query)
{
    std::vector<SceneOverlapHit> results;
    const auto& world = scene.GetRegistry();
    auto view = world.view<ColliderComponent, WorldTransformComponent>();

    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = view.get<ColliderComponent>(entity);
        if (!PassOverlapFilter(world, entity, col.GetSettings(), query)) continue;

        const auto& worldTr = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestOverlapColliderSphere(sphere, col, worldTr.worldMatrix, tempHit))
        {
            results.push_back({ entity, tempHit });
        }
    }
    return results;
}

std::vector<SceneOverlapHit> PhysicsSystem::OverlapAll(const Scene& scene, const BoundingBox& box, const OverlapQuery& query)
{
    std::vector<SceneOverlapHit> results;
    const auto& world = scene.GetRegistry();
    auto view = world.view<ColliderComponent, WorldTransformComponent>();

    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = view.get<ColliderComponent>(entity);
        if (!PassOverlapFilter(world, entity, col.GetSettings(), query)) continue;

        const auto& worldTr = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestOverlapColliderBox(box, col, worldTr.worldMatrix, tempHit))
        {
            results.push_back({ entity, tempHit });
        }
    }
    return results;
}

std::vector<SceneOverlapHit> PhysicsSystem::OverlapAll(const Scene& scene, const Capsule& capsule, const OverlapQuery& query)
{
    std::vector<SceneOverlapHit> results;
    const auto& world = scene.GetRegistry();
    auto view = world.view<ColliderComponent, WorldTransformComponent>();

    for (auto entity : view)
    {
        if (!IsComponentEnabled<ColliderComponent>(world, entity)) continue;
        const auto& col = view.get<ColliderComponent>(entity);
        if (!PassOverlapFilter(world, entity, col.GetSettings(), query)) continue;

        const auto& worldTr = view.get<WorldTransformComponent>(entity);
        HitResult tempHit{};
        if (TestOverlapColliderCapsule(capsule, col, worldTr.worldMatrix, tempHit))
        {
            results.push_back({ entity, tempHit });
        }
    }
    return results;
}
#pragma endregion
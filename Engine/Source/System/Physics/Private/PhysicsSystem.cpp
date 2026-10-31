#include "PhysicsSystem.h"
#include "PhysicsComponent.h"
#include "Scene.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/HierarchyComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Collision.h"
#include "Entity.h"
#include "TransformSystem.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Physics/ColliderComponent.h"
#include "Components/Render/TerrainComponent.h"
#include "Mesh.h"
#include <glm/gtc/matrix_transform.hpp>

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
    const auto& collider = world.get<Collider2DComponent>(entity);
    if (!collider.GetSettings().Enabled)
        return false;

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

static bool IntersectsPhysicsShapes2D(const entt::registry& world, entt::entity entityA, entt::entity entityB,
    const PhysicsShape2D& shapeA, const PhysicsShape2D& shapeB, HitResult& hit)
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

static void ResolvePhysicsContact(entt::registry& world, entt::entity entityA, entt::entity entityB, const HitResult& hit, bool resolveVelocity)
{
    auto* bodyA = world.try_get<RigidBody2DComponent>(entityA);
    auto* bodyB = world.try_get<RigidBody2DComponent>(entityB);

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

using PhysicsTriggerPair = pair<entt::entity, entt::entity>;

struct PhysicsTriggerState
{
    set<PhysicsTriggerPair> Previous;
    vector<TriggerEvent> Events;
};

static void CollectPhysicsTriggerEvents2D(Scene& scene)
{
    auto& world = scene.GetRegistry();
    auto& state = world.ctx().emplace<PhysicsTriggerState>();
    state.Events.clear();

    std::set<PhysicsTriggerPair> current;
    auto colliders = world.view<Collider2DComponent, TransformComponent, WorldTransformComponent>();

    for (auto first = colliders.begin(); first != colliders.end(); ++first)
    {
        auto second = first;
        ++second;

        for (; second != colliders.end(); ++second)
        {
            const auto handleA = *first;
            const auto handleB = *second;
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

    const vec2 gravity(0.f, -9.81f);

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
        }

        auto colliders = world.view<Collider2DComponent, TransformComponent, WorldTransformComponent>();
        constexpr int positionIterations = 8;

        for (int iteration = 0; iteration < positionIterations; ++iteration)
        {
            const bool resolveVelocity = iteration == 0;

            for (auto first = colliders.begin(); first != colliders.end(); ++first)
            {
                auto second = first;
                ++second;

                for (; second != colliders.end(); ++second)
                {
                    const auto entityA = *first;
                    const auto entityB = *second;
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

                    ResolvePhysicsContact(world, entityA, entityB, hit, resolveVelocity);
                }
            }
        }

        if (!TransformSystem::UpdateHierarchy(world))
        {
            if (auto* state = world.ctx().find<PhysicsTriggerState>())
                state->Events.clear();

            continue;
        }

        CollectPhysicsTriggerEvents2D(*scene);
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
bool PhysicsSystem::Raycast(const Scene& scene, const Ray& ray, SceneRaycastHit& outHit, f32 maxDistance)
{
    const auto allHits = RaycastAll(scene, ray, maxDistance);
    if (allHits.empty()) return false;
    outHit = allHits.front();
    return true;
}
std::vector<SceneRaycastHit> PhysicsSystem::RaycastAll(const Scene& scene, const Ray& ray, f32 maxDistance)
{
    std::vector<SceneRaycastHit> results;
    const auto& world = scene.GetRegistry();
    // 1. 3D ColliderComponent 순회
    auto view = world.view<ColliderComponent, WorldTransformComponent>();
    for (auto entity : view)
    {
        const auto* flag = world.try_get<FlagComponent>(entity);
        if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) || HasFlag(flag->flags, EEntityFlag::Dead)))
            continue;
        const auto& collider = view.get<ColliderComponent>(entity);
        if (!collider.GetSettings().Enabled)
            continue;
        const auto& worldTransform = view.get<WorldTransformComponent>(entity);
        const mat4 colliderWorld = worldTransform.worldMatrix * glm::translate(mat4(1.0f), collider.GetOffset());
        HitResult tempHit{};
        bool hitFound = false;
        if (const auto* box = std::get_if<BoxShape>(&collider.GetShape()))
        {
            if (box->HasValidParameters())
            {
                const BoundingBox b(vec3(0.0f), box->Size * 0.5f);
                hitFound = Collision::Raycast(ray, b, colliderWorld, tempHit, maxDistance);
            }
        }
        else if (const auto* sphere = std::get_if<SphereShape>(&collider.GetShape()))
        {
            if (sphere->HasValidParameters())
            {
                const vec3 worldCenter = vec3(colliderWorld[3]);
                const BoundingSphere s(worldCenter, sphere->Radius);
                hitFound = Collision::Raycast(ray, s, tempHit, maxDistance);
            }
        }
        else if (const auto* meshShape = std::get_if<MeshShape>(&collider.GetShape()))
        {
            if (meshShape->HasMesh())
            {
                if (auto* mesh = meshShape->MeshHandle.Get())
                    hitFound = Collision::Raycast(ray, *mesh, colliderWorld, tempHit, maxDistance);
            }
        }
        if (hitFound && tempHit.hasHit && tempHit.distance <= maxDistance)
        {
            results.push_back({ entity, tempHit });
        }
    }
    // 2. TerrainComponent 지형 순회
    auto terrainView = world.view<TerrainComponent, WorldTransformComponent>();
    for (auto entity : terrainView)
    {
        const auto* flag = world.try_get<FlagComponent>(entity);
        if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) || HasFlag(flag->flags, EEntityFlag::Dead)))
            continue;
        const auto& terrain = terrainView.get<TerrainComponent>(entity);
        const auto& worldTransform = terrainView.get<WorldTransformComponent>(entity);
        HitResult terrainHit{};
        if (Collision::Raycast(ray, terrain, worldTransform.worldMatrix, terrainHit, maxDistance) && terrainHit.hasHit)
        {
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
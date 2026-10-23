#include "PhysicsSystem.h"
#include "PhysicsComponent.h"
#include "Scene.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/HierarchyComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Collision.h"
#include <cmath>

EResult PhysicsSystem::Initialize(void* arg)
{
	(void)arg;
	return EResult::Success;
}

void PhysicsSystem::Free()
{
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

static bool TryBuildPhysicsBoxRect(const entt::registry& world, entt::entity entity, Rect& result)
{
    const auto& collider = world.get<Collider2DComponent>(entity);
    const auto& transform = world.get<TransformComponent>(entity);
    const auto* box = std::get_if<Box2DShape>(&collider.GetShape());

    if (!collider.GetSettings().Enabled || !box || !box->HasValidParameters())
        return false;

    const auto* hierarchy = world.try_get<HierarchyComponent>(entity);
    if (hierarchy && hierarchy->parent != entt::null)
        return false;

    const auto* flag = world.try_get<FlagComponent>(entity);
    if (flag && (!flag->IsActive() || HasFlag(flag->flags, EEntityFlag::Paused) ||
        HasFlag(flag->flags, EEntityFlag::Dead)))
        return false;

    auto candidate = transform;
    if (!candidate.HasValidAbsoluteFlags() || !candidate.SynchronizeRotation())
        return false;

    // 현재는 회전하지 않은 Box만 처리합니다.
    if (std::abs(candidate.rotation.x) > 1.0e-6f ||
        std::abs(candidate.rotation.y) > 1.0e-6f ||
        std::abs(candidate.rotation.z) > 1.0e-6f)
        return false;

    const vec2 scale(candidate.scale.x, candidate.scale.y);
    const vec2 center = vec2(candidate.position.x, candidate.position.y) + collider.GetOffset() * scale;
    const vec2 size = box->Size * glm::abs(scale);
    const vec2 min = center - size * 0.5f;

    if (!std::isfinite(min.x) || !std::isfinite(min.y) ||
        !std::isfinite(size.x) || !std::isfinite(size.y) ||
        size.x <= 0.f || size.y <= 0.f)
        return false;

    const Rect bounds(min.x, min.y, size.x, size.y);
    if (!std::isfinite(bounds.Right()) || !std::isfinite(bounds.Bottom()))
        return false;

    result = bounds;
    return true;
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

            if (body->BodyType != ERigidBodyType::Dynamic ||
                !std::isfinite(body->Mass) || body->Mass <= 0.f ||
                !std::isfinite(body->LinearVelocity.x) || !std::isfinite(body->LinearVelocity.y))
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

    glm::dvec2 velocityA = inverseMassA > 0.0 ? glm::dvec2(bodyA->LinearVelocity) : glm::dvec2(0.0);
    glm::dvec2 velocityB = inverseMassB > 0.0 ? glm::dvec2(bodyB->LinearVelocity) : glm::dvec2(0.0);
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

    // 양쪽 검증이 성공한 뒤 상태를 반영합니다.
    if (inverseMassA > 0.0)
    {
        transformA = candidateA;
        bodyA->LinearVelocity = nextVelocityA;
    }

    if (inverseMassB > 0.0)
    {
        transformB = candidateB;
        bodyB->LinearVelocity = nextVelocityB;
    }
}

void PhysicsSystem::OnFixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
    // 첫 단계에서는 PIE 복원 대상인 씬의 강체만 처리합니다.
    (void)registry;

    if (!std::isfinite(dt) || dt <= 0.f)
        return;

    const vec2 gravity(0.f, -9.81f);

    for (Scene* scene : activeScenes)
    {
        if (!scene || !scene->IsActive() || scene->IsPaused())
            continue;

        auto& world = scene->GetRegistry();
        auto view = world.view<RigidBody2DComponent, TransformComponent>();

        for (auto entity : view)
        {
            auto& body = view.get<RigidBody2DComponent>(entity);
            auto& transform = view.get<TransformComponent>(entity);

            if (body.BodyType != ERigidBodyType::Dynamic)
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
            if (!transform.SetLocalPosition(position))
                continue;

            body.LinearVelocity = velocity;
        }

        auto colliders = world.view<Collider2DComponent, TransformComponent>();
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

                    Rect boundsA, boundsB;
                    if (!TryBuildPhysicsBoxRect(world, entityA, boundsA) ||
                        !TryBuildPhysicsBoxRect(world, entityB, boundsB))
                        continue;

                    HitResult hit;
                    if (!Collision::Intersects(boundsA, boundsB, hit))
                        continue;

                    ResolvePhysicsContact(world, entityA, entityB, hit, resolveVelocity);
                }
            }
        }
    }
}
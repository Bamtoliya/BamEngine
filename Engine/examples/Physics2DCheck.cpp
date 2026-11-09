#include "PhysicsSystem.h"
#include "Scene.h"
#include "PhysicsComponent.h"
#include "TransformComponent.h"
#include "Collision.h"
#include "SystemManager.h"
#include "TransformSystem.h"
#include "WorldTransformComponent.h"

namespace
{
    using namespace Engine;

    struct ReleaseObject
    {
        void operator()(Base* value) const { if (value) value->Release(); }
    };

    using SceneOwner = std::unique_ptr<Scene, ReleaseObject>;

    int Fail(const char* message) { std::cerr << message << '\n'; return 1; }
    bool Near(f32 left, f32 right) { return glm::abs(left - right) < 0.0001f; }

    entt::entity AddBox(Scene& scene, vec2 position, ERigidBodyType type, vec2 velocity = vec2(0.f))
    {
        auto& world = scene.GetRegistry();
        const auto entity = scene.CreateEntity().GetEntityHandle();

        world.emplace<TransformComponent>(entity).position = vec3(position, 0.f);
        world.emplace<WorldTransformComponent>(entity);
        world.emplace<Collider2DComponent>(entity);

        auto& body = world.emplace<RigidBody2DComponent>(entity);
        body.BodyType = type;
        body.GravityScale = 0.f;
        body.LinearVelocity = velocity;
        return entity;
    }

    void Step(PhysicsSystem& system, Scene& scene, f32 dt)
    {
        system.OnFixedUpdate(scene.GetRegistry(), { &scene }, dt);
    }

    bool QueryOverlap2D(const Entity& first, const Entity& second, HitResult& hit)
    {
        hit.Reset();
        Scene* scene = first.GetScene();
        if (!scene || scene != second.GetScene() ||
            !TransformSystem::UpdateHierarchy(scene->GetRegistry()))
            return false;

        return PhysicsSystem::Overlap2D(first, second, hit);
    }

    const Collider2DShape shapePairs2D[][2]{
    { Box2DShape{}, Box2DShape{} },
    { Box2DShape{}, Circle2DShape{} },
    { Box2DShape{}, Capsule2DShape{} },
    { Circle2DShape{}, Box2DShape{} },
    { Circle2DShape{}, Circle2DShape{} },
    { Circle2DShape{}, Capsule2DShape{} },
    { Capsule2DShape{}, Box2DShape{} },
    { Capsule2DShape{}, Circle2DShape{} },
    { Capsule2DShape{}, Capsule2DShape{} }
    };

    int CheckCircleContacts()
    {
        HitResult hit{};
        const Circle first(vec2(0.f), 1.f);
        const Circle second(vec2(1.5f, 0.f), 1.f);

        if (!Collision::Intersects(first, second, hit) ||
            !Near(hit.denetrationDepth, 0.5f) || !Near(hit.normal.x, -1.f))
            return Fail("Circle contact failed.");

        const Rect box(-1.f, -1.f, 2.f, 2.f);
        if (!Collision::Intersects(box, second, hit) ||
            !Near(hit.denetrationDepth, 0.5f) || !Near(hit.normal.x, -1.f))
            return Fail("Box-circle contact failed.");

        if (!Collision::Intersects(second, box, hit) || !Near(hit.normal.x, 1.f))
            return Fail("Reversed circle-box normal failed.");

        const Circle inside(vec2(0.f), 0.5f);
        if (!Collision::Intersects(box, inside, hit) ||
            !Near(hit.denetrationDepth, 1.5f) || !Near(hit.normal.z, 0.f))
            return Fail("Contained circle contact failed.");

        const Circle tangent(vec2(2.f, 0.f), 1.f);
        if (!Collision::Intersects(box, tangent, hit) || !Near(hit.denetrationDepth, 0.f))
            return Fail("Tangent circle contact failed.");

        const Circle separated(vec2(3.f, 0.f), 1.f);
        if (Collision::Intersects(box, separated, hit) || hit.hasHit ||
            !Near(hit.denetrationDepth, 0.f))
            return Fail("Miss did not reset the contact result.");

        return 0;
    }

    int CheckShapeSimulation(PhysicsSystem& system)
    {
        for (const f32 angle : { 0.f, 45.f, -30.f, 90.f })
        {
            const f32 radians = glm::radians(angle);
            const vec2 direction = glm::normalize(vec2(std::cos(radians), std::sin(radians)));
            const vec2 tangent(-direction.y, direction.x);

            for (const auto& pair : shapePairs2D)
            {
                for (const bool reverseOrder : { false, true })
                {
                    SceneOwner scene(Scene::Create());
                    if (!scene) return Fail("Scene creation failed.");

                    entt::entity driver, dynamic;
                    if (reverseOrder)
                    {
                        dynamic = AddBox(*scene, direction * 1.05f, ERigidBodyType::Dynamic);
                        driver = AddBox(*scene, vec2(0.f), ERigidBodyType::Kinematic, direction);
                    }
                    else
                    {
                        driver = AddBox(*scene, vec2(0.f), ERigidBodyType::Kinematic, direction);
                        dynamic = AddBox(*scene, direction * 1.05f, ERigidBodyType::Dynamic);
                    }

                    auto& world = scene->GetRegistry();
                    if (!world.get<Collider2DComponent>(driver).SetShape(pair[0]) ||
                        !world.get<Collider2DComponent>(dynamic).SetShape(pair[1]) ||
                        !world.get<TransformComponent>(driver).SetEulerRotation(vec3(0.f, 0.f, angle)) ||
                        !world.get<TransformComponent>(dynamic).SetEulerRotation(vec3(0.f, 0.f, angle)))
                        return Fail("Rotated shape setup failed.");

                    Step(system, *scene, 0.1f);

                    const vec2 driverPosition(world.get<TransformComponent>(driver).position);
                    const vec2 dynamicPosition(world.get<TransformComponent>(dynamic).position);
                    const vec2 driverVelocity = world.get<RigidBody2DComponent>(driver).LinearVelocity;
                    const vec2 dynamicVelocity = world.get<RigidBody2DComponent>(dynamic).LinearVelocity;
                    const f32 distance = glm::dot(dynamicPosition, direction);

                    if (glm::length(driverPosition - direction * 0.1f) > 0.0001f ||
                        glm::length(driverVelocity - direction) > 0.0001f ||
                        glm::length(dynamicVelocity - direction) > 0.0001f ||
                        distance < 1.099f || distance > 1.101f ||
                        glm::abs(glm::dot(dynamicPosition, tangent)) > 0.0001f)
                        return Fail("Rotated shape pair response failed.");
                }
            }
        }

        // 반지름 확대와 음수 스케일에 따른 오프셋 반전을 검사합니다.
        {
            SceneOwner scene(Scene::Create());
            if (!scene) return Fail("Scene creation failed.");

            AddBox(*scene, vec2(0.f), ERigidBodyType::Static);
            const auto dynamic = AddBox(*scene, vec2(2.f, 0.f), ERigidBodyType::Dynamic, vec2(-1.f, 0.f));
            auto& world = scene->GetRegistry();
            auto& collider = world.get<Collider2DComponent>(dynamic);

            if (!collider.SetShape(Circle2DShape{}) || !collider.SetOffset(vec2(0.25f, 0.f)))
                return Fail("Scaled circle setup failed.");

            world.get<TransformComponent>(dynamic).scale = vec3(-2.f, 2.f, 1.f);
            Step(system, *scene, 0.1f);

            if (!Near(world.get<RigidBody2DComponent>(dynamic).LinearVelocity.x, 0.f) ||
                world.get<TransformComponent>(dynamic).position.x < 1.999f)
                return Fail("Scaled circle or reflected offset failed.");
        }

        return 0;
    }

    int CheckTriggerOverlap(PhysicsSystem& system)
    {
        for (const auto& pair : shapePairs2D)
        {
            SceneOwner scene(Scene::Create());
            if (!scene) return Fail("Scene creation failed.");

            const auto trigger = AddBox(*scene, vec2(0.f), ERigidBodyType::Static);
            const auto dynamic = AddBox(*scene, vec2(1.05f, 0.f), ERigidBodyType::Dynamic, vec2(-1.f, 0.f));
            auto& world = scene->GetRegistry();
            auto& triggerCollider = world.get<Collider2DComponent>(trigger);

            auto settings = triggerCollider.GetSettings();
            settings.IsTrigger = true;
            if (!triggerCollider.SetSettings(settings))
                return Fail("Trigger setup failed.");

            if (!triggerCollider.SetShape(pair[0]) ||
                !world.get<Collider2DComponent>(dynamic).SetShape(pair[1]))
                return Fail("Trigger shape setup failed.");

            Step(system, *scene, 0.1f);
            const auto events = PhysicsSystem::GetTriggerEvents2D(*scene);
            if (events.size() != 1 || events[0].type != ETriggerEventType::Enter)
                return Fail("Trigger shape pair did not produce an enter event.");

            auto* triggerEntity = scene->FindEntity(trigger);
            auto* dynamicEntity = scene->FindEntity(dynamic);
            if (!triggerEntity || !dynamicEntity)
                return Fail("Entity lookup failed.");

            HitResult hit{};
            if (!QueryOverlap2D(*triggerEntity, *dynamicEntity, hit) || !hit.hasHit)
                return Fail("Trigger overlap was not detected.");

            HitResult reversed{};
            if (!QueryOverlap2D(*dynamicEntity, *triggerEntity, reversed) ||
                !Near(hit.normal.x, -reversed.normal.x) || !Near(hit.normal.y, -reversed.normal.y))
                return Fail("Reversed overlap normal failed.");

            if (!Near(world.get<TransformComponent>(dynamic).position.x, 0.95f) ||
                !Near(world.get<RigidBody2DComponent>(dynamic).LinearVelocity.x, -1.f))
                return Fail("Trigger or overlap query changed motion.");

            settings.CollisionMask = 0u;
            if (!triggerCollider.SetSettings(settings))
                return Fail("Mask setup failed.");

            if (QueryOverlap2D(*triggerEntity, *dynamicEntity, hit) || hit.hasHit)
                return Fail("Overlap ignored the collision mask or retained an old hit.");

            settings.CollisionMask = 0xFFFFFFFFu;
            settings.Enabled = false;
            if (!triggerCollider.SetSettings(settings))
                return Fail("Disabled collider setup failed.");

            if (QueryOverlap2D(*triggerEntity, *dynamicEntity, hit))
                return Fail("Disabled collider participated in overlap.");

            settings.Enabled = true;
            if (!triggerCollider.SetSettings(settings))
                return Fail("Collider restore failed.");

            if (QueryOverlap2D(*triggerEntity, *triggerEntity, hit))
                return Fail("Self overlap was accepted.");

            scene->SetPaused(true);
            if (QueryOverlap2D(*triggerEntity, *dynamicEntity, hit))
                return Fail("Paused scene participated in overlap.");
        }

        return 0;
    }

    int CheckTriggerEvents(PhysicsSystem& system)
    {
        SceneOwner scene(Scene::Create());
        if (!scene) return Fail("Scene creation failed.");

        const auto trigger = AddBox(*scene, vec2(0.f), ERigidBodyType::Static);
        const auto other = AddBox(*scene, vec2(0.5f, 0.f), ERigidBodyType::Static);
        auto& world = scene->GetRegistry();

        auto settings = world.get<Collider2DComponent>(trigger).GetSettings();
        settings.IsTrigger = true;
        if (!world.get<Collider2DComponent>(trigger).SetSettings(settings))
            return Fail("Trigger setup failed.");

        const auto expect = [&](ETriggerEventType type)
            {
                Step(system, *scene, 1.f / 60.f);
                const auto events = PhysicsSystem::GetTriggerEvents2D(*scene);

                if (events.size() != 1 || events[0].type != type)
                    return false;

                const auto& event = events[0];
                return (event.entityA == trigger && event.entityB == other) ||
                    (event.entityA == other && event.entityB == trigger);
            };

        if (!expect(ETriggerEventType::Enter) || !expect(ETriggerEventType::Stay))
            return Fail("Trigger enter/stay failed.");

        world.get<TransformComponent>(other).position.x = 3.f;
        if (!expect(ETriggerEventType::Exit))
            return Fail("Trigger exit failed.");

        Step(system, *scene, 1.f / 60.f);
        if (!PhysicsSystem::GetTriggerEvents2D(*scene).empty())
            return Fail("Trigger exit repeated.");

        world.get<TransformComponent>(other).position.x = 0.5f;
        if (!expect(ETriggerEventType::Enter))
            return Fail("Trigger re-entry failed.");

        scene->SetPaused(true);
        Step(system, *scene, 1.f / 60.f);
        if (!PhysicsSystem::GetTriggerEvents2D(*scene).empty())
            return Fail("Paused scene produced trigger events.");

        scene->SetPaused(false);
        if (!expect(ETriggerEventType::Stay))
            return Fail("Pause discarded trigger history.");

        settings.Enabled = false;
        if (!world.get<Collider2DComponent>(trigger).SetSettings(settings) ||
            !expect(ETriggerEventType::Exit))
            return Fail("Disabled trigger did not exit.");

        settings.Enabled = true;
        if (!world.get<Collider2DComponent>(trigger).SetSettings(settings) ||
            !expect(ETriggerEventType::Enter))
            return Fail("Re-enabled trigger did not enter.");

        PhysicsSystem::ResetTriggerState2D(*scene);
        if (!PhysicsSystem::GetTriggerEvents2D(*scene).empty() ||
            !expect(ETriggerEventType::Enter))
            return Fail("Trigger state reset failed.");

        auto* removed = scene->FindEntity(other);
        if (!removed || !scene->DestroyEntity(*removed) || !expect(ETriggerEventType::Exit))
            return Fail("Entity removal did not produce trigger exit.");

        return 0;
    }

    int CheckCapsuleWorldShape(PhysicsSystem& system)
    {
        SceneOwner scene(Scene::Create());
        if (!scene) return Fail("Scene creation failed.");

        const auto box = AddBox(*scene, vec2(0.f), ERigidBodyType::Static);
        const auto capsule = AddBox(*scene, vec2(0.5f, 2.9f), ERigidBodyType::Static);
        auto& world = scene->GetRegistry();
        auto& collider = world.get<Collider2DComponent>(capsule);
        auto& transform = world.get<TransformComponent>(capsule);

        if (!collider.SetShape(Capsule2DShape{}) || !collider.SetOffset(vec2(0.25f)))
            return Fail("Capsule world shape setup failed.");

        transform.scale = vec3(-2.f, -2.f, 1.f);

        const auto* capsuleEntity = scene->FindEntity(capsule);
        const auto* boxEntity = scene->FindEntity(box);
        if (!capsuleEntity || !boxEntity)
            return Fail("Entity lookup failed.");

        HitResult hit{};
        if (!QueryOverlap2D(*capsuleEntity, *boxEntity, hit) ||
            !Near(hit.denetrationDepth, 0.1f) || !Near(hit.normal.y, 1.f))
            return Fail("Capsule height, scale, or reflected offset conversion failed.");

        transform.position.y = 3.1f;
        if (QueryOverlap2D(*capsuleEntity, *boxEntity, hit) || hit.hasHit)
            return Fail("Scaled capsule height is too large.");

        // 전체 높이가 지름과 같으면 중심선 길이가 0인 원이 됩니다.
        Capsule2DShape round;
        round.Height = 2.f * round.Radius;
        if (!collider.SetShape(round))
            return Fail("Round capsule setup failed.");

        transform.position.y = 2.9f;
        if (QueryOverlap2D(*capsuleEntity, *boxEntity, hit))
            return Fail("Round capsule retained a nonzero center segment.");

        if (!collider.SetShape(Capsule2DShape{}) || !collider.SetOffset(vec2(0.f)))
            return Fail("Capsule reset failed.");

        transform.position = vec3(0.f);
        transform.scale = vec3(2.f, 1.f, 1.f);
        hit.hasHit = true;
        if (QueryOverlap2D(*capsuleEntity, *boxEntity, hit) || hit.hasHit)
            return Fail("Capsule accepted nonuniform XY scale.");

        transform.scale = vec3(0.f, 0.f, 1.f);
        hit.hasHit = true;
        if (QueryOverlap2D(*capsuleEntity, *boxEntity, hit) || hit.hasHit)
            return Fail("Capsule accepted zero XY scale.");

        transform.scale = vec3(1.f);
        transform.position = vec3(0.f, 1.55f, 0.f);

        auto& body = world.get<RigidBody2DComponent>(capsule);
        body.BodyType = ERigidBodyType::Dynamic;
        body.LinearVelocity = vec2(0.f, -1.f);

        Step(system, *scene, 0.1f);

        if (transform.position.y < 1.499f || transform.position.y > 1.501f ||
            !Near(body.LinearVelocity.y, 0.f))
            return Fail("Falling capsule did not stop at its expected height.");

        return 0;
    }

    struct TriggerReaderCheck : ISystem
    {
        static TriggerReaderCheck* Create() { return new TriggerReaderCheck(); }

        std::vector<ETriggerEventType> Received;
        uint32 StepCount = 0;

        void OnPostFixedUpdate(entt::registry&, const vector<Scene*>& activeScenes, f32) override
        {
            auto* physics = SystemManager::Get().GetSystem<PhysicsSystem>();
            if (!physics || !physics->IsActive())
                return;

            ++StepCount;
            for (Scene* scene : activeScenes)
            {
                if (!scene || !scene->IsActive() || scene->IsPaused())
                    continue;

                for (const auto& event : PhysicsSystem::GetTriggerEvents2D(*scene))
                    Received.push_back(event.type);
            }
        }
    };

    int CheckPostFixedTriggerDelivery()
    {
        if (SystemManager::IsCreated())
            return Fail("System manager check requires an isolated instance.");

        auto* manager = SystemManager::Create();
        if (!manager) return Fail("System manager creation failed.");

        struct ManagerCleanup
        {
            ~ManagerCleanup() { SystemManager::Destroy(); }
        } cleanup;

        // 이벤트 소비자를 물리 시스템보다 먼저 등록합니다.
        auto* reader = manager->AddSystem<TriggerReaderCheck>();
        if (!reader || !manager->AddSystem<PhysicsSystem>())
            return Fail("Check system registration failed.");

        SceneOwner scene(Scene::Create());
        if (!scene) return Fail("Scene creation failed.");

        const auto trigger = AddBox(*scene, vec2(0.f), ERigidBodyType::Static);
        const auto other = AddBox(*scene, vec2(0.5f, 0.f), ERigidBodyType::Static);
        auto& world = scene->GetRegistry();

        auto& collider = world.get<Collider2DComponent>(trigger);
        auto settings = collider.GetSettings();
        settings.IsTrigger = true;
        if (!collider.SetSettings(settings))
            return Fail("Trigger setup failed.");

        const std::vector<Scene*> scenes{ scene.get() };

        // 일반 Update 없이 연속 고정 스텝을 실행합니다.
        manager->FixedUpdate(world, scenes, 1.f / 60.f);
        manager->FixedUpdate(world, scenes, 1.f / 60.f);

        auto* removed = scene->FindEntity(other);
        if (!removed || !scene->DestroyEntity(*removed))
            return Fail("Entity removal failed.");

        manager->FixedUpdate(world, scenes, 1.f / 60.f);

        const std::vector<ETriggerEventType> expected{
            ETriggerEventType::Enter, ETriggerEventType::Stay, ETriggerEventType::Exit
        };

        if (reader->Received != expected || reader->StepCount != 3)
            return Fail("Post-fixed event delivery lost or reordered events.");

        reader->SetActive(false);
        manager->FixedUpdate(world, scenes, 1.f / 60.f);
        if (reader->StepCount != 3)
            return Fail("Inactive system received post-fixed updates.");

        reader->SetActive(true);
        manager->FixedUpdate(world, scenes, 0.f);
        if (reader->StepCount != 3)
            return Fail("Invalid fixed step executed post-fixed updates.");

        return 0;
    }

    int CheckPlanarShapeRotation(PhysicsSystem& system)
    {
        SceneOwner scene(Scene::Create());
        if (!scene) return Fail("Scene creation failed.");

        const auto box = AddBox(*scene, vec2(0.f), ERigidBodyType::Static);
        const auto object = AddBox(*scene, vec2(1.4f, 0.f), ERigidBodyType::Static);
        auto& world = scene->GetRegistry();
        auto& collider = world.get<Collider2DComponent>(object);
        auto& transform = world.get<TransformComponent>(object);

        const auto* boxEntity = scene->FindEntity(box);
        const auto* objectEntity = scene->FindEntity(object);
        if (!boxEntity || !objectEntity)
            return Fail("Entity lookup failed.");

        if (!collider.SetShape(Capsule2DShape{}) ||
            !transform.SetEulerRotation(vec3(0.f, 0.f, 90.f)))
            return Fail("Rotated capsule setup failed.");

        HitResult hit{};
        if (!QueryOverlap2D(*objectEntity, *boxEntity, hit) ||
            !Near(hit.denetrationDepth, 0.1f) || !Near(hit.normal.x, 1.f))
            return Fail("Horizontal capsule contact failed.");

        transform.position = vec3(1.2f, 0.1f, 0.f);
        if (!transform.SetEulerRotation(vec3(0.f, 0.f, 45.f)))
            return Fail("Tilted capsule setup failed.");

        const f32 expectedDepth = 1.f + std::sqrt(0.125f) - 1.2f;
        if (!QueryOverlap2D(*objectEntity, *boxEntity, hit) ||
            !Near(hit.denetrationDepth, expectedDepth) ||
            !Near(hit.normal.x, 1.f) || !Near(hit.normal.z, 0.f))
            return Fail("Tilted capsule world conversion failed.");

        // 음수 스케일을 적용한 오프셋도 객체와 함께 회전해야 합니다.
        if (!collider.SetShape(Circle2DShape{}) || !collider.SetOffset(vec2(0.25f, 0.f)) ||
            !transform.SetEulerRotation(vec3(0.f, 0.f, 90.f)))
            return Fail("Rotated circle setup failed.");

        transform.position = vec3(0.f, 1.9f, 0.f);
        transform.scale = vec3(-2.f, -2.f, 1.f);

        if (!QueryOverlap2D(*objectEntity, *boxEntity, hit) ||
            !Near(hit.denetrationDepth, 0.1f) || !Near(hit.normal.y, 1.f))
            return Fail("Circle offset did not rotate after scaling.");

        if (!collider.SetShape(Capsule2DShape{}) || !collider.SetOffset(vec2(0.f)))
            return Fail("Capsule reset failed.");

        transform.position = vec3(0.f);
        transform.scale = vec3(1.f);

        for (const vec3 tilt : { vec3(10.f, 0.f, 0.f), vec3(0.f, 10.f, 0.f) })
        {
            if (!transform.SetEulerRotation(tilt))
                return Fail("Out-of-plane rotation setup failed.");

            hit.hasHit = true;
            if (QueryOverlap2D(*objectEntity, *boxEntity, hit) || hit.hasHit)
                return Fail("Physics accepted an out-of-plane rotation.");
        }

        if (!collider.SetShape(Box2DShape{}) ||
            !transform.SetEulerRotation(vec3(0.f, 0.f, 45.f)))
            return Fail("Rotated box setup failed.");

        if (!QueryOverlap2D(*objectEntity, *boxEntity, hit) || !hit.hasHit ||
            !Near(hit.denetrationDepth, 0.5f + std::sqrt(0.5f)) ||
            !Near(glm::length(hit.normal), 1.f))
            return Fail("Rotated box overlap failed.");

        if (!collider.SetShape(Capsule2DShape{}) ||
            !transform.SetEulerRotation(vec3(0.f, 0.f, 90.f)))
            return Fail("Moving rotated capsule setup failed.");

        transform.position = vec3(1.4f, 0.f, 0.f);

        auto& body = world.get<RigidBody2DComponent>(object);
        body.BodyType = ERigidBodyType::Dynamic;
        body.LinearVelocity = vec2(-1.f, 0.f);

        Step(system, *scene, 0.1f);

        if (transform.position.x < 1.499f || transform.position.x > 1.501f ||
            !Near(body.LinearVelocity.x, 0.f) || !Near(body.LinearVelocity.y, 0.f))
            return Fail("Rotated capsule did not stop at the box.");

        return 0;
    }

    int CheckPhysicsWorldSync(PhysicsSystem& system)
    {
        SceneOwner scene(Scene::Create());
        if (!scene) return Fail("Scene creation failed.");

        const auto driver = AddBox(*scene, vec2(0.f), ERigidBodyType::Kinematic, vec2(1.f, 0.f));
        const auto dynamic = AddBox(*scene, vec2(1.05f, 0.f), ERigidBodyType::Dynamic);
        auto& world = scene->GetRegistry();

        Step(system, *scene, 0.1f);

        for (const auto entity : { driver, dynamic })
        {
            const auto& transform = world.get<TransformComponent>(entity);
            const auto& cached = world.get<WorldTransformComponent>(entity);
            if (glm::length(vec3(cached.worldMatrix[3]) - transform.position) > 0.0001f)
                return Fail("Physics left its world component out of date.");
        }

        const f32 position = world.get<TransformComponent>(dynamic).position.x;
        if (position < 1.099f || position > 1.101f ||
            !Near(world.get<RigidBody2DComponent>(dynamic).LinearVelocity.x, 1.f))
            return Fail("Contact correction used an outdated world matrix.");

        auto& transform = world.get<TransformComponent>(dynamic);
        auto& cached = world.get<WorldTransformComponent>(dynamic);
        if (!transform.SetLocalPosition(vec3(3.f, 0.f, 0.f)) ||
            !TransformSystem::UpdateRootWorld(transform, cached))
            return Fail("Root world update failed.");

        const auto* first = scene->FindEntity(driver);
        const auto* second = scene->FindEntity(dynamic);
        if (!first || !second)
            return Fail("Entity lookup failed.");

        HitResult hit{};
        hit.hasHit = true;
        if (PhysicsSystem::Overlap2D(*first, *second, hit) || hit.hasHit)
            return Fail("Overlap did not read the current world component.");

        return 0;
    }

    int CheckRotatedBoxTrigger(PhysicsSystem& system)
    {
        const vec2 direction = glm::normalize(vec2(1.f));
        const Collider2DShape shapes[]{ Box2DShape{}, Circle2DShape{}, Capsule2DShape{} };

        for (const auto& shape : shapes)
        {
            SceneOwner scene(Scene::Create());
            if (!scene) return Fail("Scene creation failed.");

            const auto trigger = AddBox(*scene, vec2(0.f), ERigidBodyType::Static);
            const auto other = AddBox(*scene, direction * 1.05f, ERigidBodyType::Dynamic, -direction);
            auto& world = scene->GetRegistry();
            auto& collider = world.get<Collider2DComponent>(trigger);

            if (!world.get<Collider2DComponent>(other).SetShape(shape) ||
                !world.get<TransformComponent>(trigger).SetEulerRotation(vec3(0.f, 0.f, 45.f)) ||
                !world.get<TransformComponent>(other).SetEulerRotation(vec3(0.f, 0.f, 45.f)))
                return Fail("Rotated trigger setup failed.");

            auto settings = collider.GetSettings();
            settings.IsTrigger = true;
            if (!collider.SetSettings(settings))
                return Fail("Trigger settings failed.");

            const auto expect = [&](ETriggerEventType type)
                {
                    Step(system, *scene, 0.1f);
                    const auto events = PhysicsSystem::GetTriggerEvents2D(*scene);
                    return events.size() == 1 && events[0].type == type;
                };

            if (!expect(ETriggerEventType::Enter))
                return Fail("Rotated trigger enter failed.");

            const vec2 position(world.get<TransformComponent>(other).position);
            const vec2 velocity = world.get<RigidBody2DComponent>(other).LinearVelocity;
            if (glm::length(position - direction * 0.95f) > 0.0001f ||
                glm::length(velocity + direction) > 0.0001f)
                return Fail("Rotated trigger changed the passing body's motion.");

            const auto* triggerEntity = scene->FindEntity(trigger);
            const auto* otherEntity = scene->FindEntity(other);
            if (!triggerEntity || !otherEntity)
                return Fail("Entity lookup failed.");

            HitResult hit{}, reversed{};
            if (!PhysicsSystem::Overlap2D(*triggerEntity, *otherEntity, hit) ||
                !PhysicsSystem::Overlap2D(*otherEntity, *triggerEntity, reversed) ||
                !Near(hit.denetrationDepth, 0.05f) ||
                glm::length(hit.normal + reversed.normal) > 0.0001f)
                return Fail("Rotated trigger overlap query failed.");

            if (!expect(ETriggerEventType::Stay))
                return Fail("Rotated trigger stay failed.");

            world.get<TransformComponent>(other).position = vec3(direction * 3.f, 0.f);
            if (!expect(ETriggerEventType::Exit))
                return Fail("Rotated trigger exit failed.");
        }

        return 0;
    }

    int CheckRotatedBoxScale(PhysicsSystem& system)
    {
        SceneOwner scene(Scene::Create());
        if (!scene) return Fail("Scene creation failed.");

        const auto box = AddBox(*scene, vec2(-1.5f, 0.5f), ERigidBodyType::Static);
        const auto other = AddBox(*scene, vec2(1.9f, 0.f), ERigidBodyType::Static);
        auto& world = scene->GetRegistry();
        auto& collider = world.get<Collider2DComponent>(box);
        auto& transform = world.get<TransformComponent>(box);

        Box2DShape shape;
        shape.Size = vec2(2.f, 1.f);
        if (!collider.SetShape(shape) || !collider.SetOffset(vec2(0.25f, -0.5f)) ||
            !world.get<Collider2DComponent>(other).SetShape(Circle2DShape{}) ||
            !transform.SetEulerRotation(vec3(0.f, 0.f, 90.f)))
            return Fail("Scaled rotated box setup failed.");

        transform.scale = vec3(-2.f, 3.f, 1.f);
        Step(system, *scene, 1.f / 60.f);

        const auto* boxEntity = scene->FindEntity(box);
        const auto* otherEntity = scene->FindEntity(other);
        if (!boxEntity || !otherEntity)
            return Fail("Entity lookup failed.");

        HitResult hit{};
        if (!PhysicsSystem::Overlap2D(*boxEntity, *otherEntity, hit) ||
            !Near(hit.denetrationDepth, 0.1f) || !Near(hit.normal.x, -1.f))
            return Fail("Rotated box width or reflected offset failed.");

        world.get<TransformComponent>(other).position = vec3(0.f, 2.4f, 0.f);
        Step(system, *scene, 1.f / 60.f);

        if (!PhysicsSystem::Overlap2D(*boxEntity, *otherEntity, hit) ||
            !Near(hit.denetrationDepth, 0.1f) || !Near(hit.normal.y, -1.f))
            return Fail("Rotated box height failed.");

        return 0;
    }
}

int main()
{
    using namespace Engine;

    std::unique_ptr<PhysicsSystem, ReleaseObject> system(PhysicsSystem::Create());
    if (!system) return Fail("Physics system creation failed.");

    // Kinematic은 지정 속도로 이동하며 중력, 감쇠, 질량을 이동 계산에 사용하지 않습니다.
    {
        SceneOwner scene(Scene::Create());
        if (!scene) return Fail("Scene creation failed.");

        auto& world = scene->GetRegistry();
        const auto entity = AddBox(*scene, vec2(0.f), ERigidBodyType::Kinematic, vec2(2.f, -1.f));
        auto& body = world.get<RigidBody2DComponent>(entity);
        body.Mass = 0.f;
        body.GravityScale = 100.f;
        body.LinearDamping = 100.f;
        body.AccumulatedForce = vec2(100.f, 200.f);

        for (int step = 0; step < 60; ++step)
            Step(*system, *scene, 1.f / 60.f);

        const auto& position = world.get<TransformComponent>(entity).position;
        if (!Near(position.x, 2.f) || !Near(position.y, -1.f) ||
            !Near(body.LinearVelocity.x, 2.f) || !Near(body.LinearVelocity.y, -1.f))
            return Fail("Kinematic motion changed its prescribed velocity.");

        if (!Near(body.AccumulatedForce.x, 0.f) || !Near(body.AccumulatedForce.y, 0.f))
            return Fail("Kinematic force was not consumed.");

        const vec3 beforePause = position;
        body.AccumulatedForce = vec2(3.f, 4.f);
        scene->SetPaused(true);
        Step(*system, *scene, 1.f / 60.f);

        const auto& pausedPosition = world.get<TransformComponent>(entity).position;
        if (!Near(pausedPosition.x, beforePause.x) || !Near(pausedPosition.y, beforePause.y) ||
            !Near(body.AccumulatedForce.x, 3.f) || !Near(body.AccumulatedForce.y, 4.f))
            return Fail("Paused scene changed physics state.");
    }

    // 생성 순서를 바꿔 양쪽 접촉 법선 방향을 모두 검사합니다.
    for (const bool reverseOrder : { false, true })
    {
        SceneOwner scene(Scene::Create());
        if (!scene) return Fail("Scene creation failed.");

        entt::entity driver, dynamic;
        if (reverseOrder)
        {
            dynamic = AddBox(*scene, vec2(1.05f, 0.f), ERigidBodyType::Dynamic);
            driver = AddBox(*scene, vec2(0.f), ERigidBodyType::Kinematic, vec2(1.f, 0.f));
        }
        else
        {
            driver = AddBox(*scene, vec2(0.f), ERigidBodyType::Kinematic, vec2(1.f, 0.f));
            dynamic = AddBox(*scene, vec2(1.05f, 0.f), ERigidBodyType::Dynamic);
        }

        auto& world = scene->GetRegistry();
        world.get<RigidBody2DComponent>(driver).Mass = 0.f;
        Step(*system, *scene, 0.1f);

        const auto& driverBody = world.get<RigidBody2DComponent>(driver);
        const auto& dynamicBody = world.get<RigidBody2DComponent>(dynamic);
        const auto& driverPosition = world.get<TransformComponent>(driver).position;
        const auto& dynamicPosition = world.get<TransformComponent>(dynamic).position;

        if (!Near(driverPosition.x, 0.1f) || !Near(driverBody.LinearVelocity.x, 1.f) ||
            !Near(dynamicBody.LinearVelocity.x, 1.f) || dynamicPosition.x < 1.099f)
            return Fail("Kinematic contact response failed.");
    }

    // Static에 속도 값이 있어도 이동하거나 상대속도로 전달하면 안 됩니다.
    {
        SceneOwner scene(Scene::Create());
        if (!scene) return Fail("Scene creation failed.");

        const auto fixed = AddBox(*scene, vec2(0.f), ERigidBodyType::Static, vec2(7.f, 0.f));
        const auto dynamic = AddBox(*scene, vec2(1.05f, 0.f), ERigidBodyType::Dynamic, vec2(-1.f, 0.f));
        Step(*system, *scene, 0.1f);

        auto& world = scene->GetRegistry();
        if (!Near(world.get<TransformComponent>(fixed).position.x, 0.f) ||
            !Near(world.get<RigidBody2DComponent>(dynamic).LinearVelocity.x, 0.f) ||
            world.get<TransformComponent>(dynamic).position.x < 0.999f)
            return Fail("Static contact response regressed.");
    }

    if (const int result = CheckCircleContacts(); result != 0)
        return result;

    if (const int result = CheckShapeSimulation(*system); result != 0)
        return result;

    if (const int result = CheckTriggerOverlap(*system); result != 0)
        return result;

    if (const int result = CheckTriggerEvents(*system); result != 0)
        return result;

    if (const int result = CheckPostFixedTriggerDelivery(); result != 0)
        return result;

    if (const int result = CheckCapsuleWorldShape(*system); result != 0)
        return result;

    if (const int result = CheckPlanarShapeRotation(*system); result != 0)
        return result;

    if (const int result = CheckPhysicsWorldSync(*system); result != 0)
        return result;

    if (const int result = CheckRotatedBoxTrigger(*system); result != 0)
        return result;

    if (const int result = CheckRotatedBoxScale(*system); result != 0)
        return result;

    std::cout << "Physics 2D checks passed.\n";
    return 0;
}
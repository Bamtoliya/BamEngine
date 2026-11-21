#include "BuiltInPrimitiveMeshes.h"
#include "ColliderComponent.h"
#include "KinematicCharacterSystem.h"
#include "Mesh.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "TransformSystem.h"
#include "WorldTransformComponent.h"
#include <iostream>
#include <memory>

namespace
{
    using namespace Engine;

    struct ReleaseObject
    {
        void operator()(Base* object) const { if (object) object->Release(); }
    };

    bool SetPosition(entt::registry& world, entt::entity entity, const vec3& position)
    {
        auto& transform = world.get_or_emplace<TransformComponent>(entity);
        auto& worldTransform = world.get_or_emplace<WorldTransformComponent>(entity);
        return transform.SetLocalPosition(position) && TransformSystem::UpdateRootWorld(transform, worldTransform);
    }

    entt::entity AddSurface(Scene& scene, const ColliderShape& shape, const vec3& position, const vec3& scale, f32 pitch = 0.0f)
    {
        auto& world = scene.GetRegistry();
        const auto entity = scene.CreateEntity().GetEntityHandle();
        auto& transform = world.get_or_emplace<TransformComponent>(entity);
        if (!transform.SetEulerRotation(vec3(pitch, 0.0f, 0.0f)) || !transform.SetLocalScale(scale) ||
            !SetPosition(world, entity, position) || !world.emplace<ColliderComponent>(entity).SetShape(shape))
            return entt::null;
        return entity;
    }

    bool CheckJump(KinematicCharacterSystem& system, const ColliderShape& floorShape, f32 floorCenterY, f32 dt, f32 x = 0.0f)
    {
        std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
        if (!scene) return false;
        scene->SetActive(true);
        auto& world = scene->GetRegistry();
        const auto floor = scene->CreateEntity().GetEntityHandle();
        const auto character = scene->CreateEntity().GetEntityHandle();
        if (!SetPosition(world, floor, vec3(0.0f, floorCenterY, 0.0f)) ||
            !SetPosition(world, character, vec3(x, 0.0f, 0.0f)) ||
            !world.emplace<ColliderComponent>(floor).SetShape(floorShape))
            return false;

        auto& controller = world.emplace<CharacterControllerComponent>(character);
        controller.AutoUpdateAnimator = false;
        const auto& transform = world.get<TransformComponent>(character);
        const auto step = [&]() { system.OnUpdate(world, { scene.get() }, dt); };

        step();
        if (!controller.IsGrounded || controller.GroundEntity != floor) return false;
        controller.Jump();
        step();
        if (controller.IsGrounded || transform.position.y <= 0.0f) return false;

        bool descendedInAir = false;
        for (f32 elapsed = dt; elapsed < 2.0f; elapsed += dt)
        {
            const f32 previousY = transform.position.y;
            const f32 fallDistance = glm::max(0.0f, -(controller.Velocity.y + controller.Gravity * dt) * dt);
            step();
            if (controller.IsGrounded)
            {
                if (!descendedInAir || previousY > fallDistance + 0.001f || glm::abs(transform.position.y) > 0.001f ||
                    controller.GroundEntity != floor || controller.Velocity.y != 0.0f)
                    return false;

                // 기존 지지면의 낮은 계단은 스냅하고, 스냅 범위보다 낮으면 낙하를 시작한다.
                if (!SetPosition(world, character, vec3(x, 0.25f, 0.0f))) return false;
                step();
                if (!controller.IsGrounded || glm::abs(transform.position.y) > 0.001f) return false;
                if (!SetPosition(world, character, vec3(x, 0.8f, 0.0f))) return false;
                step();
                return !controller.IsGrounded && transform.position.y > 0.7f;
            }
            if (transform.position.y < -0.001f) return false;
            descendedInAir |= controller.Velocity.y < 0.0f && transform.position.y > 0.5f;
        }
        return false;
    }

    bool CheckSlope(KinematicCharacterSystem& system, f32 degrees, f32 dt, bool walkable)
    {
        std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
        if (!scene) return false;
        auto& world = scene->GetRegistry();
        const auto ramp = scene->CreateEntity().GetEntityHandle();
        const auto character = scene->CreateEntity().GetEntityHandle();
        auto& rampTransform = world.get_or_emplace<TransformComponent>(ramp);
        auto& rampWorld = world.get_or_emplace<WorldTransformComponent>(ramp);
        if (!rampTransform.SetEulerRotation(vec3(degrees, 0.0f, 0.0f)) ||
            !rampTransform.SetLocalScale(vec3(4.0f, 0.2f, 12.0f)) ||
            !TransformSystem::UpdateRootWorld(rampTransform, rampWorld))
            return false;
        world.emplace<ColliderComponent>(ramp);

        auto& controller = world.emplace<CharacterControllerComponent>(character);
        controller.AutoUpdateAnimator = false;
        const vec3 normal = rampTransform.rotation * vec3(0.0f, 1.0f, 0.0f);
        const vec3 top = rampTransform.position + normal * 0.1f;
        const vec3 start = top + normal * controller.Radius - controller.GetWorldCapsule(vec3(0.0f)).pointA;
        if (!SetPosition(world, character, start)) return false;

        const auto& transform = world.get<TransformComponent>(character);
        const vec3 downhill = glm::normalize(vec3(normal.x, 0.0f, normal.z));
        for (f32 elapsed = 0.0f; elapsed < 0.5f; elapsed += dt)
        {
            const f32 previousY = transform.position.y;
            const vec3 input = walkable || elapsed < 0.125f ? -downhill : downhill + vec3(0.5f, 0.0f, 0.0f);
            controller.Move(input);
            system.OnUpdate(world, { scene.get() }, dt);

            const auto capsule = controller.GetWorldCapsule(transform.position);
            const f32 separation = glm::dot(normal, capsule.pointA - top) - capsule.radius;
            if (controller.IsGrounded != walkable || controller.GroundEntity != ramp || glm::abs(separation) > 0.01f)
                return false;
            if (walkable ? transform.position.y <= previousY : transform.position.y >= previousY)
                return false;
        }
        return true;
    }

    bool CheckStairs(KinematicCharacterSystem& system, const MeshShape& floorShape, f32 dt)
    {
        std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
        if (!scene) return false;
        auto& world = scene->GetRegistry();
        if (AddSurface(*scene, floorShape, vec3(0.0f), vec3(24.0f, 1.0f, 24.0f)) == entt::null)
            return false;

        // SimpleTests의 높이 0.2m 간격과 비균일 배율을 재현한다.
        for (int i = 0; i < 4; ++i)
        {
            const f32 length = i == 3 ? 2.4f : 1.2f;
            const f32 z = i == 3 ? 6.7f : 2.5f + 1.2f * i;
            if (AddSurface(*scene, BoxShape{}, vec3(0.0f, 0.1f + 0.2f * i, z), vec3(3.0f, 0.2f, length)) == entt::null)
                return false;
        }

        const auto character = scene->CreateEntity().GetEntityHandle();
        if (!SetPosition(world, character, vec3(0.0f, 0.0f, 0.8f))) return false;
        const auto& transform = world.get<TransformComponent>(character);
        auto& controller = world.emplace<CharacterControllerComponent>(character);
        controller.AutoUpdateAnimator = false;

        bool descending = false;
        // 실패 단계와 위치를 남겨 어느 조건에서 멈췄는지 실행 결과만으로 구분한다.
        const auto report = [&](const char* reason, f32 elapsed, int tread = -1)
        {
            const auto& p = transform.position;
            std::cerr << "  stairs " << (descending ? "descending" : "ascending") << ": " << reason;
            if (tread >= 0) std::cerr << " tread=" << tread << " top=" << 0.2f * (tread + 1);
            std::cerr << " t=" << elapsed << " pos=(" << p.x << ", " << p.y << ", " << p.z << ") grounded=" << controller.IsGrounded
                << " radius=" << controller.Radius << '\n';
            return false;
        };
        for (f32 elapsed = 0.0f; elapsed < 5.0f; elapsed += dt)
        {
            controller.Move(vec3(0.0f, 0.0f, descending ? -1.0f : 1.0f));
            system.OnUpdate(world, { scene.get() }, dt);
            const auto& position = transform.position;
            if (descending)
            {
                // 캡슐(수직 선분 + 반지름)과 디딤판 단면(z·y 사각형)의 최단 거리로 관통을 판정한다.
                // 모서리를 넘어가는 하단 구는 발 높이가 디딤판 윗면보다 낮아도 디딤판과 닿지 않으므로 높이만으로 판정하지 않는다.
                const auto capsule = controller.GetWorldCapsule(position);
                const f32 segmentMin = glm::min(capsule.pointA.y, capsule.pointB.y);
                const f32 segmentMax = glm::max(capsule.pointA.y, capsule.pointB.y);
                const f32 contactLimit = controller.Radius - 0.01f;
                for (int i = 0; i < 4; ++i)
                {
                    const f32 nearZ = 1.9f + 1.2f * i;
                    const f32 farZ = nearZ + (i == 3 ? 2.4f : 1.2f);
                    const f32 bottom = 0.2f * i;
                    const f32 top = bottom + 0.2f;
                    const f32 dz = glm::max(glm::max(nearZ - capsule.pointA.z, 0.0f), capsule.pointA.z - farZ);
                    const f32 dy = glm::max(glm::max(bottom - segmentMax, 0.0f), segmentMin - top);
                    if (dz * dz + dy * dy < contactLimit * contactLimit)
                        return report("capsule penetrates a tread", elapsed, i);
                }
                if (position.z <= 1.0f)
                {
                    if (!controller.IsGrounded || glm::abs(position.y) > 0.01f)
                        return report("did not settle on the floor", elapsed);
                    return true;
                }
            }
            else if (position.z >= 6.6f)
            {
                if (!controller.IsGrounded || glm::abs(position.y - 0.8f) > 0.01f)
                    return report("reached the top tread without standing on it", elapsed);
                descending = true;
            }
        }
        return report("timed out", 5.0f);
    }

    bool CheckRampEntryStop(KinematicCharacterSystem& system, const MeshShape& floorShape, f32 dt)
    {
        std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
        if (!scene || AddSurface(*scene, floorShape, vec3(0.0f), vec3(16.0f, 1.0f, 16.0f)) == entt::null)
            return false;
        const auto ramp = AddSurface(*scene, BoxShape{}, vec3(0.0f, 0.9134f, 0.0f), vec3(2.5f, 0.2f, 4.0f), 30.0f);
        if (ramp == entt::null) return false;
        auto& world = scene->GetRegistry();
        const auto character = scene->CreateEntity().GetEntityHandle();
        if (!SetPosition(world, character, vec3(0.0f, 0.0f, 3.0f))) return false;
        const auto& transform = world.get<TransformComponent>(character);
        auto& controller = world.emplace<CharacterControllerComponent>(character);
        controller.AutoUpdateAnimator = false;

        for (f32 elapsed = 0.0f; elapsed < 1.5f; elapsed += dt)
        {
            controller.Move(vec3(0.0f, 0.0f, -1.0f));
            system.OnUpdate(world, { scene.get() }, dt);
            if (!controller.IsGrounded || controller.GroundEntity != ramp || transform.position.y < 0.03f)
                continue;

            const vec3 stopPosition = transform.position;
            for (f32 stoppedFor = 0.0f; stoppedFor < 0.5f; stoppedFor += dt)
                system.OnUpdate(world, { scene.get() }, dt);
            return controller.IsGrounded && controller.GroundEntity == ramp &&
                glm::length(transform.position - stopPosition) <= 0.03f;
        }
        return false;
    }

    bool CheckAirborneRampContact(KinematicCharacterSystem& system, f32 dt)
    {
        std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
        if (!scene) return false;
        const auto ramp = AddSurface(*scene, BoxShape{}, vec3(0.0f), vec3(2.5f, 0.2f, 4.0f), 30.0f);
        if (ramp == entt::null) return false;
        auto& world = scene->GetRegistry();
        const auto character = scene->CreateEntity().GetEntityHandle();
        auto& controller = world.emplace<CharacterControllerComponent>(character);
        controller.AutoUpdateAnimator = false;
        controller.Gravity = 0.0f;
        const vec3 normal = world.get<TransformComponent>(ramp).rotation * vec3(0.0f, 1.0f, 0.0f);
        const vec3 start = normal * (0.11f + controller.Radius) - controller.GetWorldCapsule(vec3(0.0f)).pointA;
        if (!SetPosition(world, character, start)) return false;

        const vec3 uphill = -glm::normalize(vec3(normal.x, 0.0f, normal.z));
        constexpr f32 jumpSpeed = 0.01f;
        controller.Velocity = uphill * controller.WalkSpeed + vec3(0.0f, jumpSpeed, 0.0f);
        controller.Move(uphill);
        system.OnUpdate(world, { scene.get() }, dt);
        const f32 expectedY = start.y + jumpSpeed * dt;
        return !controller.IsGrounded && glm::abs(world.get<TransformComponent>(character).position.y - expectedY) <= 0.001f;
    }

    bool CheckOccludedLanding(KinematicCharacterSystem& system, const MeshShape& floorShape, f32 dt)
    {
        std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
        if (!scene) return false;
        const auto floor = AddSurface(*scene, floorShape, vec3(0.0f), vec3(1.0f));
        const auto underside = AddSurface(*scene, floorShape, vec3(0.0f, 0.25f, 0.0f), vec3(1.0f), 180.0f);
        if (floor == entt::null || underside == entt::null) return false;
        auto& world = scene->GetRegistry();
        const auto character = scene->CreateEntity().GetEntityHandle();
        if (!SetPosition(world, character, vec3(0.0f, 0.06f, 0.0f))) return false;
        auto& controller = world.emplace<CharacterControllerComponent>(character);
        controller.AutoUpdateAnimator = false;
        controller.Velocity.y = -8.0f;

        system.OnUpdate(world, { scene.get() }, dt);
        const f32 landedY = world.get<TransformComponent>(character).position.y;
        return controller.IsGrounded && controller.GroundEntity == floor && glm::abs(landedY) <= 0.001f;
    }

    bool CheckRampSlideToMeshFloor(KinematicCharacterSystem& system, const MeshShape& floorShape, f32 dt)
    {
        std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
        if (!scene) return false;
        const auto floor = AddSurface(*scene, floorShape, vec3(0.0f), vec3(16.0f, 1.0f, 16.0f));
        // 평지가 경사로의 하단을 가로지른다. 경사로 접촉이 남아 있어도 평지 아래로 내려갈 수 없어야 한다.
        const auto ramp = AddSurface(*scene, BoxShape{}, vec3(0.0f, 0.5f, 0.0f), vec3(2.5f, 0.2f, 4.0f), 60.0f);
        if (floor == entt::null || ramp == entt::null) return false;

        auto& world = scene->GetRegistry();
        const auto character = scene->CreateEntity().GetEntityHandle();
        auto& controller = world.emplace<CharacterControllerComponent>(character);
        controller.AutoUpdateAnimator = false;
        controller.Velocity.y = -3.0f;
        const auto& rampTransform = world.get<TransformComponent>(ramp);
        const vec3 normal = rampTransform.rotation * vec3(0.0f, 1.0f, 0.0f);
        const vec3 contact = rampTransform.position + normal * (0.1f + controller.Radius);
        const vec3 start = contact - controller.GetWorldCapsule(vec3(0.0f)).pointA + vec3(0.0f, 0.35f, 0.0f);
        if (!SetPosition(world, character, start)) return false;

        const auto& transform = world.get<TransformComponent>(character);
        bool touchedRampWhileAirborne = false;
        for (f32 elapsed = 0.0f; elapsed < 2.0f; elapsed += dt)
        {
            system.OnUpdate(world, { scene.get() }, dt);
            if (transform.position.y < -0.001f)
                return false;
            if (controller.IsGrounded && controller.GroundEntity == floor)
                return touchedRampWhileAirborne && glm::abs(transform.position.y) <= 0.001f;
            if (!controller.IsGrounded && controller.GroundEntity == ramp)
                touchedRampWhileAirborne = true;
        }
        return false;
    }

    bool CheckRampToFloorSoftLanding(KinematicCharacterSystem& system, const MeshShape& floorShape, f32 dt)
    {
        auto runTransition = [&](f32 pitch, const vec3& rampPos, const vec3& rampScale, const vec3& moveDir) -> bool
        {
            std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
            if (!scene) return false;
            const auto floor = AddSurface(*scene, floorShape, vec3(0.0f), vec3(20.0f, 1.0f, 20.0f));
            const auto ramp = AddSurface(*scene, BoxShape{}, rampPos, rampScale, pitch);
            if (floor == entt::null || ramp == entt::null) return false;

            auto& world = scene->GetRegistry();
            const auto character = scene->CreateEntity().GetEntityHandle();
            auto& controller = world.emplace<CharacterControllerComponent>(character);
            controller.AutoUpdateAnimator = false;

            const auto& rampTransform = world.get<TransformComponent>(ramp);
            const vec3 normal = rampTransform.rotation * vec3(0.0f, 1.0f, 0.0f);
            const vec3 top = rampTransform.position + normal * (rampScale.y * 0.5f);
            const vec3 start = top + normal * controller.Radius - controller.GetWorldCapsule(vec3(0.0f)).pointA;
            if (!SetPosition(world, character, start)) return false;

            const auto& transform = world.get<TransformComponent>(character);
            const f32 maxStepDrop = 6.5f * dt + 0.015f;
            bool touchedRamp = false;
            bool inTransition = false;
            int transitionFrames = 0;

            for (f32 elapsed = 0.0f; elapsed < 2.0f; elapsed += dt)
            {
                const f32 previousY = transform.position.y;
                controller.Move(moveDir);
                system.OnUpdate(world, { scene.get() }, dt);

                if (transform.position.y < -0.001f)
                    return false;

                if (!touchedRamp)
                {
                    if (controller.IsGrounded && controller.GroundEntity == ramp &&
                        glm::abs(controller.GroundAngle - pitch) <= 1.0f)
                    {
                        touchedRamp = true;
                    }
                    continue;
                }

                if (!inTransition && (controller.GroundEntity != ramp || controller.GroundAngle > pitch + 1.0f))
                {
                    inTransition = true;
                }

                if (inTransition)
                {
                    ++transitionFrames;
                    if (previousY - transform.position.y > maxStepDrop)
                        return false;
                    if (controller.IsGrounded && controller.GroundEntity == floor &&
                        glm::abs(transform.position.y) <= 0.002f && controller.GroundAngle < 1.0f)
                    {
                        return transitionFrames >= 2;
                    }
                }
            }
            return false;
        };

        // 1) 45도 경사로 내리막 끝단(단차 약 0.19m) -> 평지 전환 연착륙
        const vec3 downhill45 = glm::normalize(vec3(0.0f, 0.0f, 1.0f));
        if (!runTransition(45.0f, vec3(0.0f, 1.68f, 0.0f), vec3(3.0f, 0.2f, 4.0f), downhill45))
            return false;

        // 2) 30도 경사로 측면 단차(약 0.20m) -> 평지 전환 stepDown 연착륙
        if (!runTransition(30.0f, vec3(0.0f, 0.18f, 0.0f), vec3(1.6f, 0.2f, 4.0f), vec3(1.0f, 0.0f, 0.0f)))
            return false;

        return true;
    }

    bool CheckMovingPlatformInheritance(KinematicCharacterSystem& system, f32 dt)
    {
        std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
        if (!scene) return false;
        const auto platform = AddSurface(*scene, BoxShape{}, vec3(0.0f, -0.25f, 0.0f), vec3(6.0f, 0.5f, 6.0f));
        if (platform == entt::null) return false;

        auto& world = scene->GetRegistry();
        const auto character = scene->CreateEntity().GetEntityHandle();
        const vec3 initialCharPos(1.2f, 0.0f, 0.0f);
        if (!SetPosition(world, character, initialCharPos)) return false;

        auto& controller = world.emplace<CharacterControllerComponent>(character);
        controller.AutoUpdateAnimator = false;

        auto& platformTransform = world.get<TransformComponent>(platform);
        auto& platformWorld = world.get<WorldTransformComponent>(platform);
        const auto& charTransform = world.get<TransformComponent>(character);

        system.OnUpdate(world, { scene.get() }, dt);
        if (!controller.IsGrounded || controller.GroundEntity != platform)
            return false;

        const vec3 expectedLocalOffset = initialCharPos - platformTransform.position;
        vec3 platformPos = platformTransform.position;
        f32 platformYaw = 0.0f;

        for (f32 elapsed = 0.0f; elapsed < 0.5f; elapsed += dt)
        {
            platformPos += vec3(1.5f * dt, 0.0f, 0.6f * dt);
            platformYaw += 45.0f * dt;
            if (!platformTransform.SetLocalPosition(platformPos) ||
                !platformTransform.SetEulerRotation(vec3(0.0f, platformYaw, 0.0f)) ||
                !TransformSystem::UpdateRootWorld(platformTransform, platformWorld))
                return false;

            system.OnUpdate(world, { scene.get() }, dt);
            if (!controller.IsGrounded || controller.GroundEntity != platform ||
                glm::abs(charTransform.position.y) > 0.01f)
                return false;
        }

        const vec3 actualLocalOffset = glm::inverse(platformTransform.rotation) *
            (charTransform.position - platformTransform.position);
        if (glm::length(actualLocalOffset - expectedLocalOffset) > 0.02f)
            return false;

        const vec3 charForward = charTransform.rotation * vec3(0.0f, 0.0f, 1.0f);
        const vec3 platformForward = platformTransform.rotation * vec3(0.0f, 0.0f, 1.0f);
        return glm::dot(charForward, platformForward) >= 0.99f;
    }
}

int main()
{
    using namespace Engine;
    if (!ResourceManager::Create()) return 1;
    struct ResourceScope { ~ResourceScope() { ResourceManager::Destroy(); } } resourceScope;

    auto& resources = ResourceManager::Get();
    RegisterBuiltInPrimitiveMeshes(resources);
    MeshShape meshFloor;
    meshFloor.MeshHandle = resources.GetResourceHandle<Mesh>(L"PlaneMesh");
    std::unique_ptr<KinematicCharacterSystem, ReleaseObject> system(KinematicCharacterSystem::Create());
    if (!system || !meshFloor.HasMesh()) return 1;

    BoxShape boxFloor;
    boxFloor.Size = vec3(8.0f, 1.0f, 8.0f);
    for (const f32 dt : { 1.0f / 60.0f, 1.0f / 120.0f })
    {
        if (!CheckJump(*system, boxFloor, -0.5f, dt) || !CheckJump(*system, meshFloor, 0.0f, dt) ||
            !CheckJump(*system, boxFloor, -0.5f, dt, 4.2f))
        {
            std::cerr << "Ground contact was missed or accepted before the falling step reached the floor. dt=" << dt << '\n';
            return 1;
        }
        if (!CheckSlope(*system, 30.0f, dt, true) || !CheckSlope(*system, 45.0f, dt, true) ||
            !CheckSlope(*system, 60.0f, dt, false))
        {
            std::cerr << "Ramp movement lost its support or moved in the wrong direction. dt=" << dt << '\n';
            return 1;
        }
        if (!CheckStairs(*system, meshFloor, dt))
        {
            std::cerr << "Stair traversal stalled or the capsule dropped through a supporting tread. dt=" << dt << '\n';
            return 1;
        }
        if (!CheckRampEntryStop(*system, meshFloor, dt))
        {
            std::cerr << "Ramp entry or stopping after input release failed. dt=" << dt << '\n';
            return 1;
        }
        if (!CheckAirborneRampContact(*system, dt))
        {
            std::cerr << "Airborne side contact changed the jump height. dt=" << dt << '\n';
            return 1;
        }
        if (!CheckOccludedLanding(*system, meshFloor, dt))
        {
            std::cerr << "An invalid ray hit hid the landing surface below it. dt=" << dt << '\n';
            return 1;
        }
        if (!CheckRampToFloorSoftLanding(*system, meshFloor, dt))
        {
            std::cerr << "Ramp-to-floor transition dropped abruptly or penetrated the floor. dt=" << dt << '\n';
            return 1;
        }
        if (!CheckMovingPlatformInheritance(*system, dt))
        {
            std::cerr << "Moving platform position or rotation inheritance failed. dt=" << dt << '\n';
            return 1;
        }
    }
    for (const f32 dt : { 1.0f / 30.0f, 1.0f / 60.0f, 1.0f / 120.0f })
    {
        if (!CheckRampSlideToMeshFloor(*system, meshFloor, dt))
        {
            std::cerr << "A fall onto a steep ramp did not stop at the mesh floor. dt=" << dt << '\n';
            return 1;
        }
    }
    return 0;
}

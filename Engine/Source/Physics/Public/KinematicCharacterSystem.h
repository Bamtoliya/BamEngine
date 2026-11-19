#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include "CharacterControllerComponent.h"
#include "HitResult.h"

BEGIN(Engine)
class Scene;
struct SceneRaycastHit;

class ENGINE_API KinematicCharacterSystem : public ISystem
{
private:
    KinematicCharacterSystem() = default;
    virtual ~KinematicCharacterSystem() = default;
    EResult Initialize(void* arg = nullptr);

public:
    static KinematicCharacterSystem* Create(void* arg = nullptr);
    virtual void Free() override;
    virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
    virtual void OnFixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;

public:
    static bool SweepCharacter(const Scene& scene, const CharacterControllerComponent& kcc, const vec3& fromPos, const vec3& direction, f32 distance, HitResult& outHit, entt::entity ignoreEntity);
    static bool DetectGround(const Scene& scene, const CharacterControllerComponent& kcc, const vec3& currentPos, HitResult& outGroundHit, entt::entity ignoreEntity, entt::entity* outHitEntity = nullptr);

private:
    enum class SweepPurpose { Movement, GroundSupport };
    struct GroundSupport;
    static bool SweepCharacterContact(const Scene& scene, const CharacterControllerComponent& kcc, const vec3& fromPos,
        const vec3& direction, f32 distance, SceneRaycastHit& outContact, entt::entity ignoreEntity, SweepPurpose purpose);
    static bool FindGroundSupport(const Scene& scene, const CharacterControllerComponent& kcc, const vec3& currentPos,
        f32 maxDrop, GroundSupport& outGround, entt::entity ignoreEntity);
    static void UpdateCharacter(Scene& scene, entt::entity entity, CharacterControllerComponent& kcc, f32 dt);
};
END

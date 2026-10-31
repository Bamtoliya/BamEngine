#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include "TriggerEvent.h"
#include "Structs/HitResult.h"
#include "Structs/Ray.h"

BEGIN(Engine)
class Entity;
class Scene;

struct ENGINE_API SceneRaycastHit
{
	entt::entity Entity = entt::null;
	HitResult Hit;
};

struct ENGINE_API RaycastQuery
{
	f32 maxDistance = FLT_MAX;
	uint32 layerMask = 0xFFFFFFFFu;
	bool includeTrigger = false;
	entt::entity ignoreEntity = entt::null;
};

class ENGINE_API PhysicsSystem : public ISystem
{
private:
	PhysicsSystem() {}
	virtual ~PhysicsSystem() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static PhysicsSystem* Create(void* arg = nullptr);
	virtual void Free();
public:
	virtual void OnFixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
public:
	static bool Overlap2D(const Entity& entityA, const Entity& entityB, HitResult& outHitResult);
public:
	static std::span<const TriggerEvent> GetTriggerEvents2D(const Scene& scene);
	static void ResetTriggerState2D(Scene& scene);
public:
	static bool Raycast(const Scene& scene, const Ray& ray, SceneRaycastHit& outHit, const RaycastQuery& query);
	static std::vector<SceneRaycastHit> RaycastAll(const Scene& scene, const Ray& ray, const RaycastQuery& query);
	// 기존 호출부 하위 호환 오버로드
	static bool Raycast(const Scene& scene, const Ray& ray, SceneRaycastHit& outHit, f32 maxDistance = FLT_MAX)
	{
		RaycastQuery query{};
		query.maxDistance = maxDistance;
		return Raycast(scene, ray, outHit, query);
	}
	static std::vector<SceneRaycastHit> RaycastAll(const Scene& scene, const Ray& ray, f32 maxDistance = FLT_MAX)
	{
		RaycastQuery query{};
		query.maxDistance = maxDistance;
		return RaycastAll(scene, ray, query);
	}
};
END
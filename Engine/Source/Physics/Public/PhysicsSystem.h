#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include "TriggerEvent.h"
#include "HitResult.h"
#include "Ray.h"
#include "PhysicsSettings.h"
#include <functional>

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
	virtual void Free() override;
public:
	virtual void OnFixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
public:
	static const PhysicsSettings& GetSettings();
	static void SetSettings(const PhysicsSettings& settings);
public:
	static bool Overlap2D(const Entity& entityA, const Entity& entityB, HitResult& outHitResult);
	static bool Overlap3D(const Scene& scene, entt::entity entityA, entt::entity entityB, HitResult& outHitResult);
public:
	static std::span<const TriggerEvent> GetTriggerEvents2D(const Scene& scene);
	static std::span<const TriggerEvent> GetTriggerEvents3D(const Scene& scene);
	static void ResetTriggerState2D(Scene& scene);
	static void ResetTriggerState3D(Scene& scene);

	// 전역 트리거 리스너 등록
	using GlobalTriggerCallback = std::function<void(const TriggerEvent&)>;
	static void RegisterGlobalTriggerCallback(GlobalTriggerCallback callback);
	static void ClearGlobalTriggerCallbacks();

	// 2D & 3D 통합 이벤트 디스패처
	static void DispatchTriggerEvents(Scene& scene, const std::vector<TriggerEvent>& events);
public:
	static bool Raycast(const Scene& scene, const Ray& ray, SceneRaycastHit& outHit, const RaycastQuery& query);
	static std::vector<SceneRaycastHit> RaycastAll(const Scene& scene, const Ray& ray, const RaycastQuery& query);
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

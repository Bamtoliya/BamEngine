#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include "TriggerEvent.h"
#include "CollisionCallbackComponent.h"
#include "HitResult.h"
#include "Ray.h"
#include "BoundingSphere.h"
#include "BoundingBox.h"
#include "Capsule.h"
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

struct ENGINE_API SceneSweepHit
{
	entt::entity Entity = entt::null;
	HitResult Hit;
};
using SweepHit = SceneSweepHit;

struct ENGINE_API SweepQuery
{
	f32 maxDistance = FLT_MAX;
	uint32 layerMask = 0xFFFFFFFFu;
	bool includeTrigger = false;
	entt::entity ignoreEntity = entt::null;
};

struct ENGINE_API SceneOverlapHit
{
	entt::entity Entity = entt::null;
	HitResult Hit;
};
using OverlapHit = SceneOverlapHit;

struct ENGINE_API OverlapQuery
{
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

	static std::span<const CollisionEvent> GetCollisionEvents3D(const Scene& scene);
	static void ResetCollisionState3D(Scene& scene);

	// 전역 3D 충돌 리스너 등록
	using GlobalCollisionCallback = std::function<void(const CollisionEvent&)>;
	static void RegisterGlobalCollisionCallback(GlobalCollisionCallback callback);
	static void ClearGlobalCollisionCallbacks();

	// 3D 충돌 이벤트 디스패처
	static void DispatchCollisionEvents(Scene& scene, const std::vector<CollisionEvent>& events);
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
public:
	// ── 씬 레벨 3D 스윕(Sweep) API ──
	static bool Sweep(const Scene& scene, const BoundingSphere& sphere, const vec3& direction, SceneSweepHit& outHit, const SweepQuery& query);
	static bool Sweep(const Scene& scene, const BoundingBox& box, const vec3& direction, SceneSweepHit& outHit, const SweepQuery& query);
	static bool Sweep(const Scene& scene, const Capsule& capsule, const vec3& direction, SceneSweepHit& outHit, const SweepQuery& query);

	static std::vector<SceneSweepHit> SweepAll(const Scene& scene, const BoundingSphere& sphere, const vec3& direction, const SweepQuery& query);
	static std::vector<SceneSweepHit> SweepAll(const Scene& scene, const BoundingBox& box, const vec3& direction, const SweepQuery& query);
	static std::vector<SceneSweepHit> SweepAll(const Scene& scene, const Capsule& capsule, const vec3& direction, const SweepQuery& query);

	static bool SweepSphere(const Scene& scene, const vec3& center, f32 radius, const vec3& direction, SceneSweepHit& outHit, const SweepQuery& query)
	{
		return Sweep(scene, BoundingSphere(center, radius), direction, outHit, query);
	}
	static bool SweepBox(const Scene& scene, const vec3& center, const vec3& halfExtents, const vec3& direction, SceneSweepHit& outHit, const SweepQuery& query)
	{
		return Sweep(scene, BoundingBox(center - halfExtents, center + halfExtents), direction, outHit, query);
	}
	static bool SweepCapsule(const Scene& scene, const vec3& bottom, const vec3& top, f32 radius, const vec3& direction, SceneSweepHit& outHit, const SweepQuery& query)
	{
		return Sweep(scene, Capsule(bottom, top, radius), direction, outHit, query);
	}

	// ── 씬 레벨 3D 오버랩(OverlapAll) API ──
	static std::vector<SceneOverlapHit> OverlapAll(const Scene& scene, const BoundingSphere& sphere, const OverlapQuery& query);
	static std::vector<SceneOverlapHit> OverlapAll(const Scene& scene, const BoundingBox& box, const OverlapQuery& query);
	static std::vector<SceneOverlapHit> OverlapAll(const Scene& scene, const Capsule& capsule, const OverlapQuery& query);

	static std::vector<SceneOverlapHit> OverlapSphereAll(const Scene& scene, const vec3& center, f32 radius, const OverlapQuery& query)
	{
		return OverlapAll(scene, BoundingSphere(center, radius), query);
	}
	static std::vector<SceneOverlapHit> OverlapBoxAll(const Scene& scene, const vec3& center, const vec3& halfExtents, const OverlapQuery& query)
	{
		return OverlapAll(scene, BoundingBox(center - halfExtents, center + halfExtents), query);
	}
	static std::vector<SceneOverlapHit> OverlapCapsuleAll(const Scene& scene, const vec3& bottom, const vec3& top, f32 radius, const OverlapQuery& query)
	{
		return OverlapAll(scene, Capsule(bottom, top, radius), query);
	}
};
END

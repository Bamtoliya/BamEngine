#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include "TriggerEvent.h"

BEGIN(Engine)
class Entity;
struct HitResult;
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
private:

};
END
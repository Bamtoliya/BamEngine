#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include <entt/entity/entity.hpp>
#include <string_view>
#include <vector>

BEGIN(Engine)

class Scene;

class ENGINE_API GameplayCombatSystem : public ISystem
{
private:
	GameplayCombatSystem() = default;
	virtual ~GameplayCombatSystem() = default;
	EResult Initialize(void* arg = nullptr);

public:
	static GameplayCombatSystem* Create(void* arg = nullptr);
	virtual void Free() override;

	virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
	virtual uint8 GetExecutionPhases() const override { return static_cast<uint8>(ESystemPhase::Gameplay); }

public:
	// ── 전투 로직 정적 호출 API ──
	static void ProcessSceneCombat(Scene& scene, f32 dt);
	static void ProcessRegistryCombat(entt::registry& world, f32 dt, const Scene* scene = nullptr);

	// ── 상호작용 정적 질의 API ──
	static entt::entity FindNearestInteractable(const Scene& scene, const vec3& interactorPos, std::string_view requiredTag = {});
	static bool TryInteractNearest(Scene& scene, const vec3& interactorPos, entt::entity* outInteracted = nullptr, std::string_view requiredTag = {});
};

END

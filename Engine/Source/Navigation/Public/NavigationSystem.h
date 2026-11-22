#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include "NavGrid.h"
#include <entt/entity/entity.hpp>
#include <memory>
#include <unordered_map>

BEGIN(Engine)

class Scene;

class ENGINE_API NavigationSystem : public ISystem
{
private:
	NavigationSystem() = default;
	virtual ~NavigationSystem() = default;
	EResult Initialize(void* arg = nullptr);

	static inline std::unordered_map<const Scene*, std::shared_ptr<NavGrid>> s_SceneNavGrids;

public:
	static NavigationSystem* Create(void* arg = nullptr);
	virtual void Free() override;

	virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
	virtual uint8 GetExecutionPhases() const override { return static_cast<uint8>(ESystemPhase::Gameplay); }

	// NavGrid 관리 API
	static void SetSceneNavGrid(const Scene* scene, std::shared_ptr<NavGrid> grid);
	static std::shared_ptr<NavGrid> GetSceneNavGrid(const Scene* scene);
	static void RemoveSceneNavGrid(const Scene* scene);
	static void ClearNavGrids();

	// 씬 및 레지스트리 단위 처리 정적 API
	static void ProcessScene(Scene& scene, f32 dt);
	static void ProcessRegistry(entt::registry& world, f32 dt, const Scene* scene = nullptr);
};

END

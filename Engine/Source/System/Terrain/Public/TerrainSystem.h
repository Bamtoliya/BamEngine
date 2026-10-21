#pragma once

#include "System.h"

BEGIN(Engine)
class ENGINE_API TerrainSystem : public ISystem
{
private:
	TerrainSystem() = default;
	virtual ~TerrainSystem() = default;
	EResult Initialize(void* arg = nullptr);

public:
	static TerrainSystem* Create(void* arg = nullptr);
	virtual void Free() override;

public:
	virtual void OnUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) override;
	virtual void OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) override;

private:
	void EnsureTerrainResources(class Scene* scene, entt::entity entity, struct TerrainComponent& terrain);
	void UpdateTerrainHeightmap(TerrainComponent& terrain);
	void UpdateTerrainSplatmap(TerrainComponent& terrain);
};
END
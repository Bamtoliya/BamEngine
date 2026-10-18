#pragma once

#include "System.h"

BEGIN(Engine)
class ENGINE_API MeshRenderSystem : public ISystem
{
private:
	MeshRenderSystem() {}
	virtual ~MeshRenderSystem() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static MeshRenderSystem* Create(void* arg = nullptr);
	virtual void Free() override;
public:
	virtual void OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) override;
};
END
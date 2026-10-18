#pragma once

#include "System.h"

BEGIN(Engine)
class ENGINE_API SpriteRenderSystem : public ISystem
{
private:
	SpriteRenderSystem() {}
	virtual ~SpriteRenderSystem() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static SpriteRenderSystem* Create(void* arg = nullptr);
	virtual void Free() override;
public:
	virtual void OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) override;
};
END
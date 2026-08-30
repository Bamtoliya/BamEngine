#pragma once

#include "System.h"

BEGIN(Engine)
class ENGINE_API SpriteRenderSystem : public ISystem
{
	DECLARE_SINGLETON(SpriteRenderSystem)
private:
	SpriteRenderSystem() {}
	virtual ~SpriteRenderSystem() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
public:
	virtual void OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) override;
};
END
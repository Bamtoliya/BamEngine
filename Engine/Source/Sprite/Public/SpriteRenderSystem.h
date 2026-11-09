#pragma once

#include "System.h"
#include "SpriteBatcher.h"

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
	virtual void OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;

	SpriteBatcher& GetBatcher() { return m_Batcher; }
	const SpriteBatcher& GetBatcher() const { return m_Batcher; }

private:
	SpriteBatcher m_Batcher;
};
END

#pragma once

#include "System.h"
#include "SpriteBatcher.h"

BEGIN(Engine)

class ENGINE_API TilemapRenderSystem final : public ISystem
{
private:
	TilemapRenderSystem() = default;
	virtual ~TilemapRenderSystem() = default;
	EResult Initialize(void* arg = nullptr);

public:
	static TilemapRenderSystem* Create(void* arg = nullptr);
	virtual void Free() override;

	virtual void OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;

	SpriteBatcher& GetBatcher() { return m_Batcher; }
	const SpriteBatcher& GetBatcher() const { return m_Batcher; }

private:
	SpriteBatcher m_Batcher;
};

END

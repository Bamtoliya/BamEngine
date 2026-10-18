#pragma once

#include "System.h"

BEGIN(Engine)

class ENGINE_API LightSystem final : public ISystem
{
private:
	LightSystem() = default;
	~LightSystem() override = default;
	EResult Initialize(void* arg = nullptr);
public:
	static LightSystem* Create(void* arg = nullptr);
	virtual void Free();

public:
	void OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt) override;
	static void UpdateLights(entt::registry& registry);
};

END
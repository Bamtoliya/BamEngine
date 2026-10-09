#pragma once

#include "System.h"

BEGIN(Engine)

class ENGINE_API LightSystem final : public ISystem
{
private:
	LightSystem() = default;

public:
	~LightSystem() override = default;
	static LightSystem* Create();

public:
	void OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt) override;
	static void UpdateLights(entt::registry& registry);
};

END
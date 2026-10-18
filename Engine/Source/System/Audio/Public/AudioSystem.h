#pragma once

#include "System.h"

BEGIN(Engine)

class ENGINE_API AudioSystem final : public ISystem
{
private:
	AudioSystem() = default;
	~AudioSystem() override = default;
	EResult Initialize(void* arg = nullptr);

public:
	static AudioSystem* Create(void* arg = nullptr);
	virtual void Free() override;

public:
	void OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt) override;

private:
	static void UpdateListeners(entt::registry& registry);
	static void UpdateSources(entt::registry& registry);
};

END
#pragma once

#include "System.h"

BEGIN(Engine)

class ENGINE_API AnimationSystem : public ISystem
{
private:
	AnimationSystem() = default;
	virtual ~AnimationSystem() = default;
	EResult Initialize(void* arg = nullptr) { return EResult::Success; }

public:
	static AnimationSystem* Create(void* arg = nullptr);
	virtual void Free() override {}
	virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
};

END
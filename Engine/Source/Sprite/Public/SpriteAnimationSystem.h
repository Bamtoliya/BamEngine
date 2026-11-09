#pragma once

#include "System.h"

BEGIN(Engine)

class ENGINE_API SpriteAnimationSystem : public ISystem
{
private:
	SpriteAnimationSystem() = default;
	virtual ~SpriteAnimationSystem() = default;
	EResult Initialize(void* arg = nullptr) { return EResult::Success; }

public:
	static SpriteAnimationSystem* Create(void* arg = nullptr);
	virtual void Free() override {}

public:
	virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
	virtual uint8 GetExecutionPhases() const override { return static_cast<uint8>(ESystemPhase::All); }
};

END
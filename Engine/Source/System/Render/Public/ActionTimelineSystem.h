#pragma once

#include "System.h"

BEGIN(Engine)

class ENGINE_API ActionTimelineSystem : public ISystem
{
private:
	ActionTimelineSystem() = default;
	virtual ~ActionTimelineSystem() = default;
	EResult Initialize(void* arg = nullptr) { return EResult::Success; }

public:
	static ActionTimelineSystem* Create(void* arg = nullptr);
	virtual void Free() override {}
	virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
	virtual uint8 GetExecutionPhases() const override { return static_cast<uint8>(ESystemPhase::All); }
};

END
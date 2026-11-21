#pragma once

#include "System.h"

namespace Engine
{
	struct ActionTimelineEvent;
}

class BaselineChallengeSystem final : public Engine::ISystem
{
public:
	static BaselineChallengeSystem* Create() { return new BaselineChallengeSystem(); }

	void OnUpdate(entt::registry&, const std::vector<Engine::Scene*>& activeScenes, f32 dt) override;
	static void OnActionTimelineEvent(const Engine::ActionTimelineEvent& event);
	static void ClearSceneState();
};

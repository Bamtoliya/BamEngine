#pragma once

#include "System.h"

namespace Engine
{
	class ENGINE_API SplineFollowSystem final : public ISystem
	{
	private:
		SplineFollowSystem() = default;
		virtual ~SplineFollowSystem() override = default;

	public:
		static SplineFollowSystem* Create(void* arg = nullptr);
		virtual void Free() override {}

		virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
		virtual uint8 GetExecutionPhases() const override { return static_cast<uint8>(ESystemPhase::Gameplay); }
	};
}
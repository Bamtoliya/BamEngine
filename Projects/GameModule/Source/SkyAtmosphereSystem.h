#pragma once

#include "System.h"
#include <vector>

namespace Engine
{
	class Scene;
}

class SkyAtmosphereSystem final : public Engine::ISystem
{
public:
	static SkyAtmosphereSystem* Create() { return new SkyAtmosphereSystem(); }
	virtual ~SkyAtmosphereSystem() = default;

public:
	void OnUpdate(entt::registry& globalRegistry, const std::vector<Engine::Scene*>& activeScenes, f32 dt) override;
};
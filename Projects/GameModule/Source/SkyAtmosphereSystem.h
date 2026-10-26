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
	// 모듈 로드 시 구름 텍스처 생성 및 SkyMaterial 바인딩을 1회 수행하는 정적 헬퍼
	static void InitializeResources();

public:
	void OnUpdate(entt::registry& globalRegistry, const std::vector<Engine::Scene*>& activeScenes, f32 dt) override;
};
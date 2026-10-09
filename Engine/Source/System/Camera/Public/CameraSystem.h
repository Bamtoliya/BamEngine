#pragma once

#include "System.h"

BEGIN(Engine)

class ENGINE_API CameraSystem final : public ISystem
{
private:
	CameraSystem() = default;

public:
	~CameraSystem() override = default;
	static CameraSystem* Create();

public:
	void OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt) override;
	static void UpdateCameras(entt::registry& registry);
};

END
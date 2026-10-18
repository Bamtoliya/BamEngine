#pragma once

#include "System.h"

BEGIN(Engine)

class ENGINE_API CameraSystem final : public ISystem
{
private:
	CameraSystem() = default;
	~CameraSystem() override = default;
	EResult Initialize(void* arg = nullptr);
public:
	static CameraSystem* Create(void* arg = nullptr);
	virtual void Free();

public:
	void OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt) override;
	static void UpdateCameras(entt::registry& registry);
};

END
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
	void OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt) override;
	static void UpdateCameras(entt::registry& registry, uint32 viewportWidth = 0, uint32 viewportHeight = 0);
	static bool UpdateCamera(entt::registry& registry, entt::entity entity, uint32 viewportWidth = 0, uint32 viewportHeight = 0);
};

END
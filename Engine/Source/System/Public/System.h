#pragma once

#include "Base.h"
#include "Scene.h"

BEGIN(Engine)
class ENGINE_API ISystem : public Base
{
public:
	virtual ~ISystem() = default;

	virtual void OnAwake() {}
	virtual void OnInitialize() {}

	virtual void OnUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) {}
	virtual void OnFixedUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) {}
	virtual void OnLateUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) {}

	virtual void OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) {}

	virtual void OnDestroy() {}
	virtual void OnFree() {}

	bool IsActive() const { return m_IsActive; }
	void SetActive(bool active) { m_IsActive = active; }
	void ToggleActive() { m_IsActive = !m_IsActive; }

private:
	bool m_IsActive = true;
};
END
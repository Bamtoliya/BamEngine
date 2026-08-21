#pragma once

#include "Engine_Includes.h"
#include "System.h"

BEGIN(Engine)
class ENGINE_API PhysicsSystem : public ISystem
{
private:
	EResult Initialize(void* arg = nullptr);
	virtual void Free();
public:
	static PhysicsSystem* Create(void* arg = nullptr);

public:
	virtual void OnFixedUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt) override;
private:

};
END
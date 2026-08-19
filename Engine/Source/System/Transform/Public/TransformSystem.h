#pragma once

#include "Engine_Includes.h"
#include "System.h"

BEGIN(Engine)

class ENGINE_API TransformSystem : public ISystem
{
private:
	EResult Initialize(void* arg = nullptr);
	virtual void Free();
public:
	static TransformSystem* Create(void* arg = nullptr);

public:
	virtual void OnUpdate(entt::registry& registry, f32 dt) override;
	static void UpdateHierarchy(entt::registry& registry);
private:
	static void UpdateChildWorldMatrix(entt::registry& registry, entt::entity entity, const mat4& parentWorldMatrix);
private:

};
END
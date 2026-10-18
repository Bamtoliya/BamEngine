#include "PhysicsSystem.h"
#include "PhysicsComponent.h"

EResult PhysicsSystem::Initialize(void* arg)
{
	(void)arg;
	return EResult::Success;
}

void PhysicsSystem::Free()
{
}

PhysicsSystem* PhysicsSystem::Create(void* arg)
{
	PhysicsSystem* instance = new PhysicsSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void PhysicsSystem::OnFixedUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
}
#include "PhysicsSystem.h"
#include "PhysicsComponent.h"

EResult PhysicsSystem::Initialize(void* arg)
{
	return EResult();
}

void PhysicsSystem::Free()
{
}

PhysicsSystem* PhysicsSystem::Create(void* arg)
{
	PhysicsSystem* instance = new PhysicsSystem();
	if (instance->Initialize(arg) != EResult::Success)
	{
		Safe_Delete(instance);
		return nullptr;
	}
	return instance;
}

void PhysicsSystem::OnFixedUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
}
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
	return nullptr;
}

void PhysicsSystem::OnFixedUpdate(entt::registry& registry, f32 dt)
{
}

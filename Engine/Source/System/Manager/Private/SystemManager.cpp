#pragma once
#include "SystemManager.h"
#include "Scene.h"

IMPLEMENT_SINGLETON(SystemManager)

EResult SystemManager::Initialize(void* arg)
{
	return EResult();
}

void SystemManager::Free()
{
}

void SystemManager::FixedUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
	for (ISystem* system : m_Systems)
	{
		if (system->IsActive())
			system->OnFixedUpdate(registry, activeScenes, dt);
	}
}

void SystemManager::Update(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
	for (ISystem* system : m_Systems)
	{
		if (system->IsActive())
			system->OnUpdate(registry, activeScenes, dt);
	}
}

void SystemManager::LateUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
	for (ISystem* system : m_Systems)
	{
		if (system->IsActive())
			system->OnLateUpdate(registry, activeScenes, dt);
	}
}

void SystemManager::Sumbit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
	for (ISystem* system : m_Systems)
	{
		if (system->IsActive())
			system->OnSubmit(registry, activeScenes, dt);
	}
}

void SystemManager::InitializeSystems()
{
	for (ISystem* system : m_Systems)
	{
		system->OnAwake();
		system->OnInitialize();
	}
}

void SystemManager::ClearSystems()
{
	for (ISystem* system : m_Systems)
	{
		Safe_Delete(system);
	}
	m_Systems.clear();
	m_SystemMap.clear();
}



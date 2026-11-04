#pragma once
#include "SystemManager.h"
#include "Scene.h"

IMPLEMENT_SINGLETON(SystemManager)

EResult SystemManager::Initialize(void* arg)
{
	return EResult::Success;
}

void SystemManager::Free()
{
	for (ISystem* system : m_Systems)
	{
		Safe_Release(system);
	}
}

void SystemManager::FixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	if (!std::isfinite(dt) || dt <= 0.f)
		return;

	for (ISystem* system : m_Systems)
	{
		if (system->IsActive())
			system->OnFixedUpdate(registry, activeScenes, dt);
	}

	for (ISystem* system : m_Systems)
	{
		if (system->IsActive())
			system->OnPostFixedUpdate(registry, activeScenes, dt);
	}
}

void SystemManager::Update(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt, uint8 phaseMask)
{
	for (ISystem* system : m_Systems)
	{
		if (system->IsActive() && (system->GetExecutionPhases() & phaseMask))
			system->OnUpdate(registry, activeScenes, dt);
	}
}

void SystemManager::LateUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	for (ISystem* system : m_Systems)
	{
		if (system->IsActive())
			system->OnLateUpdate(registry, activeScenes, dt);
	}
}

void SystemManager::Sumbit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
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
		Safe_Release(system);
	}
	m_Systems.clear();
	m_SystemMap.clear();
}



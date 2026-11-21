#pragma once
#include "SystemManager.h"
#include "Scene.h"
#ifdef BAM_EDITOR
#include "Logger.h"
#include <chrono>
#include <typeinfo>
#include <utility>
#endif

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
	m_UpdatePhaseMask = phaseMask;
#ifdef BAM_EDITOR
	const bool measure = std::exchange(m_UpdateTimingRequested, false);
	using Clock = std::chrono::steady_clock;
#endif
	for (ISystem* system : m_Systems)
	{
		if (!system->IsActive() || !(system->GetExecutionPhases() & phaseMask)) continue;

#ifdef BAM_EDITOR
		const auto start = measure ? Clock::now() : Clock::time_point{};
#endif
		system->OnUpdate(registry, activeScenes, dt);
#ifdef BAM_EDITOR
		if (measure)
		{
			const double elapsed = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
			if (elapsed >= 5.0)
				BAM_LOG(Info, "Engine", "UpdateTiming", "First PIE update: {} took {:.2f} ms", typeid(*system).name(), elapsed);
		}
#endif
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



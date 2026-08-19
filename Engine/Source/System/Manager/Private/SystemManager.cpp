#pragma once
#include "SystemManager.h"

IMPLEMENT_SINGLETON(SystemManager)

EResult SystemManager::Initialize(void* arg)
{
	return EResult();
}

void SystemManager::Free()
{
}

void SystemManager::FixedUpdate(entt::registry& registry, f32 dt)
{
}

void SystemManager::Update(entt::registry& registry, f32 dt)
{
}

void SystemManager::LateUpdate(entt::registry& registry, f32 dt)
{
}

void SystemManager::Render(entt::registry& registry)
{
}

template<typename T, typename ...Args>
inline T* SystemManager::RegisterSystem(Args && ...args)
{
	return nullptr;
}

template<typename T>
inline void SystemManager::UnregisterSystem()
{
}

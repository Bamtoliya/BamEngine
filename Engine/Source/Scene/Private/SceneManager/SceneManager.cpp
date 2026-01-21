#pragma once

#include "SceneManager.h"

IMPLEMENT_SINGLETON(SceneManager)

#pragma region Constructor&Destructor
EResult SceneManager::Initialize(void* arg)
{
	return EResult::Success;
}

void SceneManager::Free()
{
	Safe_Release(m_ActiveScene);
	for (auto& scene : m_Scenes)
	{
		Safe_Release(scene);
	}
	m_Scenes.clear();
}
#pragma endregion

#pragma region Loop
void SceneManager::FixedUpdate(f32 dt)
{
	if(m_ActiveScene)
		m_ActiveScene->FixedUpdate(dt);
}
void SceneManager::Update(f32 dt)
{
	if(m_ActiveScene)
		m_ActiveScene->Update(dt);
}
void SceneManager::LateUpdate(f32 dt)
{
	if(m_ActiveScene)
		m_ActiveScene->LateUpdate(dt);
}
EResult SceneManager::Render(f32 dt)
{
	if(m_ActiveScene)
		return m_ActiveScene->Render(dt);
	return EResult::Success;
}
#pragma endregion



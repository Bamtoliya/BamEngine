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
	Safe_Release(m_CurrentScene);
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
	if(m_CurrentScene)
		m_CurrentScene->FixedUpdate(dt);
}
void SceneManager::Update(f32 dt)
{
	if(m_CurrentScene)
		m_CurrentScene->Update(dt);
}
void SceneManager::LateUpdate(f32 dt)
{
	if(m_CurrentScene)
		m_CurrentScene->LateUpdate(dt);
}
#pragma endregion


#pragma region Scene Management
EResult SceneManager::NewScene(void* arg)
{
	if (m_CurrentScene)
	{
		Safe_Release(m_CurrentScene);
		m_CurrentScene = nullptr;
	}
	
	Scene* newScene = Scene::Create(arg);
	if (!newScene) return EResult::Fail;

	m_CurrentScene = newScene;
	m_Scenes.push_back(m_CurrentScene);
	
	return EResult::Success;
}
#pragma endregion




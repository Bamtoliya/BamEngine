#pragma once

#include "SceneManager.h"
#include "Archives.h"

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
EResult SceneManager::OpenScene(Scene* newScene)
{
	if (!newScene) return EResult::Fail;

	if (m_CurrentScene)
	{
		Safe_Release(m_CurrentScene);
		m_CurrentScene = nullptr;
	}
	m_CurrentScene = newScene;

	return EResult::Success;
}
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

EResult SceneManager::SaveScene(Archive& archive, const wstring& filePath)
{
	if (!m_CurrentScene) return EResult::Fail;

	string pathStr = WStrToStr(filePath);
	if(archive.PushScope(m_CurrentScene->GetTypeInfo().Name.data()))
	{
		m_CurrentScene->Serialize(archive);
		archive.PopScope();
	}

	if (archive.SaveToFile(pathStr))
	{
		return EResult::Success;
	}

	return EResult::Fail;
}

EResult SceneManager::LoadScene(Archive& archive, const wstring& filePath)
{
	string pathStr = WStrToStr(filePath);
	if (!archive.LoadFromFile(pathStr))
	{
		return EResult::Fail;
	}

	Scene* newScene = Scene::Create();
	if (!newScene) return EResult::Fail;
	
	if (archive.PushScope(newScene->GetTypeInfo().Name.data()))
	{
		newScene->Deserialize(archive);
		archive.PopScope();
	}

	return OpenScene(newScene);
}
#pragma endregion




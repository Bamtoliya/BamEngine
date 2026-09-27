#pragma once

#include "SceneManager.h"
#include "Archives.h"
#include "LightManager.h"
#include "CollisionManager.h"
#include "CameraManager.h"

IMPLEMENT_SINGLETON(SceneManager)

#pragma region Constructor&Destructor
EResult SceneManager::Initialize(void* arg)
{
	return EResult::Success;
}

void SceneManager::Free()
{
	m_CurrentScene = nullptr;
	m_ActiveScenes.clear();

	for (auto& scene : m_Scenes)
	{
		Safe_Release(scene);
	}

	m_Scenes.clear();
}
#pragma endregion

//#pragma region Loop
//void SceneManager::FixedUpdate(f32 dt)
//{
//	if(m_CurrentScene)
//		m_CurrentScene->FixedUpdate(dt);
//}
//void SceneManager::Update(f32 dt)
//{
//	if(m_CurrentScene)
//		m_CurrentScene->Update(dt);
//}
//void SceneManager::LateUpdate(f32 dt)
//{
//	if(m_CurrentScene)
//		m_CurrentScene->LateUpdate(dt);
//}
//#pragma endregion


#pragma region Scene Management
EResult SceneManager::OpenScene(Scene* newScene)
{
	if (!newScene)
		return EResult::InvalidArgument;

	if (m_CurrentScene == newScene)
		return EResult::Success;

	if (IsFailure(CloseScene()))
		return EResult::Fail;

	if (IsFailure(AddScene(newScene)))
		return EResult::Fail;

	m_CurrentScene = newScene;
	return EResult::Success;
}
EResult SceneManager::CloseScene()
{
	if (!m_CurrentScene)
		return EResult::Success;

	Scene* closingScene = m_CurrentScene;
	const EResult result = RemoveScene(closingScene);

	if (IsFailure(result))
		return result;

	LightManager::Get().ClearLightSources();
	CollisionManager::Get().ClearColliders();
	CollisionManager::Get().ClearRigidBodies();
	CameraManager::Get().ClearCameras();

	return EResult::Success;
}
EResult SceneManager::NewScene(void* arg)
{
	Scene* newScene = Scene::Create(arg);

	if (!newScene)
		return EResult::Fail;

	const EResult result = OpenScene(newScene);

	if (IsFailure(result))
		Safe_Release(newScene);

	return result;
}

EResult SceneManager::SaveScene(Archive& archive, const wstring& filePath)
{
	if (!m_CurrentScene) return EResult::Fail;

	string pathStr = WStrToStr(filePath);
	if(archive.PushScope(entt::resolve(m_CurrentScene->GetTypeID()).info().name().data()))
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

	CloseScene();
	Scene* newScene = Scene::Create();
	if (!newScene) return EResult::Fail;
	
	if (archive.PushScope(entt::resolve(newScene->GetTypeID()).info().name().data()))
	{
		newScene->Deserialize(archive);
		archive.PopScope();
	}

	return OpenScene(newScene);
}
EResult SceneManager::AddScene(Scene* scene)
{
	if (!scene) return EResult::InvalidArgument;

	if (std::find(m_Scenes.begin(), m_Scenes.end(), scene) != m_Scenes.end())
	{
		return EResult::Success;
	}

	m_Scenes.push_back(scene);
	return EResult::Success;
}
EResult SceneManager::RemoveScene(Scene* scene)
{
	auto it = std::find(m_Scenes.begin(), m_Scenes.end(), scene);

	if (it == m_Scenes.end())
		return EResult::Fail;

	if (m_CurrentScene == scene)
		m_CurrentScene = nullptr;

	m_ActiveScenes.clear();
	m_Scenes.erase(it);

	Safe_Release(scene);
	return EResult::Success;
}
vector<Scene*> SceneManager::GetActiveScenes()
{
	m_ActiveScenes.clear();

	for (Scene* scene : m_Scenes)
	{
		if (scene && scene->IsActive())
		{
			m_ActiveScenes.push_back(scene);
		}
	}

	return m_ActiveScenes;
}
#pragma endregion




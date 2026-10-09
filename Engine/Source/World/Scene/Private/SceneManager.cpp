#pragma once

#include "SceneManager.h"
#include "Archives.h"
#include "LightManager.h"
#include "CollisionManager.h"
#include "CameraManager.h"
#include <new>

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
	if (!newScene) return EResult::InvalidArgument;
	if (m_CurrentScene == newScene) return EResult::Success;

	const bool alreadyRegistered = std::find(m_Scenes.begin(), m_Scenes.end(), newScene) != m_Scenes.end();

	const EResult addResult = AddScene(newScene);
	if (addResult != EResult::Success)
		return addResult;

	const EResult closeResult = CloseScene();
	if (closeResult != EResult::Success)
	{
		if (!alreadyRegistered)
			RemoveScene(newScene);

		return closeResult;
	}

	m_CurrentScene = newScene;
	return EResult::Success;
}
EResult SceneManager::CloseScene()
{
	if (!m_CurrentScene)
		return EResult::Success;

	Scene* closingScene = m_CurrentScene;
	const EResult result = RemoveScene(closingScene);

	if (result != EResult::Success)
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
	if (!newScene) return EResult::Fail;

	const EResult result = OpenScene(newScene);
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
	if (!archive.IsReading() || filePath.empty())
		return EResult::InvalidArgument;

	// 엔티티 복원이 구현되기 전에는 기존 씬을 교체하지 않는다.
	return EResult::NotImplemented;
}

EResult SceneManager::AddScene(Scene* scene)
{
	if (!scene) return EResult::InvalidArgument;

	if (std::find(m_Scenes.begin(), m_Scenes.end(), scene) != m_Scenes.end())
		return EResult::Success;

	try
	{
		m_Scenes.push_back(scene);
	}
	catch (const std::bad_alloc&)
	{
		return EResult::OutOfMemory;
	}

	Safe_AddRef(scene);
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




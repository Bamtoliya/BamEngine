#pragma once

#include "SceneManager.h"
#include <archive/ArchiveInterface.h>
#include "LightManager.h"
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

EResult SceneManager::SaveScene(archive::IArchive& ar, const wstring& filePath)
{
	if (filePath.empty())
		return EResult::InvalidArgument;

	if (!m_CurrentScene)
		return EResult::Fail;

	auto result = m_CurrentScene->WriteArchive(ar);
	if (result)
		result = ar.SaveToFile(std::filesystem::path(filePath));

	if (!result)
	{
		ENGINE_LOG_ERROR("Scene save failed: code={}, field={}, path={}",
			static_cast<int>(result.Code), result.Field, WStrToStr(filePath));
		return EResult::Fail;
	}

	return EResult::Success;
}

EResult SceneManager::LoadScene(archive::IArchive& ar, const wstring& filePath)
{
	if (filePath.empty())
		return EResult::InvalidArgument;

	Scene* candidate = nullptr;
	auto result = ar.LoadFromFile(std::filesystem::path(filePath));
	if (result) result = Scene::CreateFromArchive(ar, candidate);
	if (!result)
	{
		ENGINE_LOG_ERROR("Scene load failed: code={}, field={}, path={}",
			static_cast<int>(result.Code), result.Field, WStrToStr(filePath));
		return EResult::Fail;
	}

	const EResult openResult = OpenScene(candidate);
	Safe_Release(candidate);
	return openResult;
}

EResult SceneManager::SavePrefab(archive::IArchive& ar, const wstring& filePath, const Entity& rootEntity)
{
	if (filePath.empty())
		return EResult::InvalidArgument;

	if (!m_CurrentScene)
		return EResult::Fail;

	auto result = m_CurrentScene->WritePrefabArchive(ar, rootEntity);
	if (result)
		result = ar.SaveToFile(std::filesystem::path(filePath));

	if (!result)
	{
		ENGINE_LOG_ERROR("Prefab save failed: code={}, field={}, path={}",
			static_cast<int>(result.Code), result.Field, WStrToStr(filePath));
		return EResult::Fail;
	}

	return EResult::Success;
}

EResult SceneManager::InstantiatePrefab(
	archive::IArchive& ar, const wstring& filePath, Entity*& outRootEntity, Entity* parent)
{
	if (filePath.empty() || outRootEntity != nullptr)
		return EResult::InvalidArgument;

	if (!m_CurrentScene)
		return EResult::Fail;

	auto result = ar.LoadFromFile(std::filesystem::path(filePath));
	if (result)
		result = m_CurrentScene->InstantiatePrefabArchive(ar, outRootEntity, parent);

	if (!result)
	{
		ENGINE_LOG_ERROR("Prefab load failed: code={}, field={}, path={}",
			static_cast<int>(result.Code), result.Field, WStrToStr(filePath));
		return EResult::Fail;
	}

	return EResult::Success;
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




#pragma once

#include "SceneManager.h"
#include "ActionTimelineSystem.h"
#if BAM_EDITOR
#include "AnimationSystem.h"
#include "GraphSystem.h"
#include "SystemManager.h"
#endif
#include <archive/ArchiveInterface.h>
#include <archive/JsonArchive.h>
#include "LightManager.h"
#include "PrefabInstanceComponent.h"
#include "ScenePersistenceComponent.h"
#include "WorldComponents.h"
#include "PlatformPath.h"
#include <chrono>
#include <functional>

#pragma region Constructor&Destructor
IMPLEMENT_SINGLETON(SceneManager)

EResult SceneManager::Initialize(void* /*arg*/)
{
	return EResult::Success;
}

void SceneManager::Free()
{
	m_CurrentScene = nullptr;
	m_ActiveScenes.clear();
	m_AdditiveScenes.clear();

	for (auto& scene : m_Scenes)
	{
		Safe_Release(scene);
	}

	m_Scenes.clear();
}
#pragma endregion

#pragma region Scene Management
EResult SceneManager::OpenScene(Scene* newScene)
{
	if (!newScene) return EResult::InvalidArgument;
	if (m_CurrentScene == newScene) return EResult::Success;

	const bool alreadyRegistered = std::find(m_Scenes.begin(), m_Scenes.end(), newScene) != m_Scenes.end();

	const EResult addResult = AddScene(newScene);
	if (addResult != EResult::Success)
		return addResult;

	if (m_CurrentScene)
	{
		TransferPersistentEntities(*m_CurrentScene, *newScene);
	}

	const EResult closeResult = CloseScene();
	if (closeResult != EResult::Success)
	{
		if (!alreadyRegistered)
			RemoveScene(newScene);

		return closeResult;
	}

	m_CurrentScene = newScene;
	ActionTimelineSystem::PrepareSceneResources(*newScene);
#if BAM_EDITOR
	AnimationSystem::PrepareSceneControllerResources(*newScene);
	if (SystemManager::IsCreated())
	{
		if (auto* graphs = SystemManager::Get().GetSystem<GraphSystem>())
			graphs->PrepareSceneGraphs(*newScene);
	}
#endif
	return EResult::Success;
}

EResult SceneManager::ChangeScene(Scene* newScene)
{
	return OpenScene(newScene);
}

EResult SceneManager::ChangeScene(archive::IArchive& ar, const wstring& filePath)
{
	return LoadScene(ar, filePath);
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

EResult SceneManager::SavePrefab(archive::IArchive& ar, const wstring& filePath, const Entity& rootEntity)
{
	if (filePath.empty())
		return EResult::InvalidArgument;
	if (PlatformPath::IsReadOnlyTarget(filePath))
		return EResult::AccessDenied;

	if (!m_CurrentScene)
		return EResult::Fail;

	auto result = m_CurrentScene->WritePrefabArchive(ar, rootEntity);
	if (result)
		result = ar.SaveToFile(PlatformPath::ResolveUri(filePath));

	if (!result)
	{
		ENGINE_LOG_ERROR("Prefab save failed: code={}, field={}, path={}",
			static_cast<int32>(result.Code), result.Field, WStrToStr(filePath));
		return EResult::Fail;
	}

	return EResult::Success;
}

EResult SceneManager::InstantiatePrefab(archive::IArchive& ar, const wstring& filePath, Entity*& outRootEntity, Entity* parent)
{
	if (filePath.empty() || outRootEntity != nullptr)
		return EResult::InvalidArgument;

	if (!m_CurrentScene)
		return EResult::Fail;

	const auto physical = PlatformPath::ResolveUri(filePath);
	auto result = ar.LoadFromFile(physical);
	if (result)
		result = m_CurrentScene->InstantiatePrefabArchive(ar, outRootEntity, parent);

	if (!result)
	{
		ENGINE_LOG_ERROR("Prefab load failed: code={}, field={}, path={}",
			static_cast<int32>(result.Code), result.Field, WStrToStr(filePath));
		return EResult::Fail;
	}

	if (outRootEntity)
	{
		auto& comp = m_CurrentScene->GetRegistry().emplace_or_replace<PrefabInstanceComponent>(outRootEntity->GetEntityHandle());
		const auto uri = PlatformPath::ToVirtualUri(physical);
		comp.prefabAssetPath = PlatformPath::IsProjectUri(uri) ? uri : filePath;
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

	for (auto addIt = m_AdditiveScenes.begin(); addIt != m_AdditiveScenes.end(); )
	{
		if (addIt->second == scene)
			addIt = m_AdditiveScenes.erase(addIt);
		else
			++addIt;
	}

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

#pragma region Additive SubScene Streaming
EResult SceneManager::LoadSceneAdditive(const std::filesystem::path& scenePath, const std::string& subSceneName)
{
	if (subSceneName.empty() || scenePath.empty())
		return EResult::InvalidArgument;

	if (m_AdditiveScenes.find(subSceneName) != m_AdditiveScenes.end())
	{
		ENGINE_LOG_WARN("Additive scene already loaded with name: {}", subSceneName);
		return EResult::Fail;
	}

	std::filesystem::path targetPath = scenePath;
	if (targetPath.is_relative())
		targetPath = PlatformPath::ResolveProjectRelative(targetPath);

	if (!std::filesystem::exists(targetPath))
	{
		const std::string ext = targetPath.extension().string();
		if (ext == ".json")
		{
			const std::string stem = targetPath.stem().string();
			if (!stem.ends_with(".bamscene"))
			{
				const auto candidatePath = targetPath.parent_path() / (stem + ".bamscene.json");
				if (std::filesystem::exists(candidatePath))
					targetPath = candidatePath;
			}
		}
	}

	if (!std::filesystem::exists(targetPath))
	{
		ENGINE_LOG_ERROR("Additive scene file not found: {}", targetPath.string());
		return EResult::Fail;
	}

	archive::JsonArchive ar;
	auto result = ar.LoadFromFile(targetPath);
	if (!result)
	{
		ENGINE_LOG_ERROR("Additive scene archive load failed: {}", targetPath.string());
		return EResult::Fail;
	}

	Scene* subScene = nullptr;
	result = Scene::CreateFromArchive(ar, subScene);
	if (!result || !subScene)
	{
		ENGINE_LOG_ERROR("Additive scene create from archive failed: {}", targetPath.string());
		return EResult::Fail;
	}

	subScene->SetName(StrToWStr(subSceneName));
	const EResult regRes = RegisterAdditiveScene(subSceneName, subScene);
	Safe_Release(subScene);

	return regRes;
}

EResult SceneManager::RegisterAdditiveScene(const std::string& subSceneName, Scene* scene)
{
	if (subSceneName.empty() || !scene)
		return EResult::InvalidArgument;

	if (m_AdditiveScenes.find(subSceneName) != m_AdditiveScenes.end())
		return EResult::Fail;

	const EResult addResult = AddScene(scene);
	if (addResult != EResult::Success)
		return addResult;

	scene->SetActive(true);
	m_AdditiveScenes[subSceneName] = scene;
	return EResult::Success;
}

EResult SceneManager::UnloadAdditiveScene(const std::string& subSceneName)
{
	auto it = m_AdditiveScenes.find(subSceneName);
	if (it == m_AdditiveScenes.end())
		return EResult::Fail;

	Scene* scene = it->second;
	m_AdditiveScenes.erase(it);

	return RemoveScene(scene);
}

EResult SceneManager::UnloadAllAdditiveScenes()
{
	std::vector<std::string> names;
	names.reserve(m_AdditiveScenes.size());
	for (const auto& [name, scene] : m_AdditiveScenes)
		names.push_back(name);

	EResult lastResult = EResult::Success;
	for (const auto& name : names)
	{
		const EResult r = UnloadAdditiveScene(name);
		if (r != EResult::Success)
			lastResult = r;
	}

	return lastResult;
}

Scene* SceneManager::GetAdditiveScene(const std::string& subSceneName)
{
	auto it = m_AdditiveScenes.find(subSceneName);
	if (it != m_AdditiveScenes.end())
		return it->second;

	return nullptr;
}

std::vector<std::string> SceneManager::GetAdditiveSceneNames() const
{
	std::vector<std::string> names;
	names.reserve(m_AdditiveScenes.size());
	for (const auto& [name, scene] : m_AdditiveScenes)
		names.push_back(name);

	return names;
}
#pragma endregion

#pragma region Persistent Entities
EResult SceneManager::MarkDontDestroyOnLoad(Scene& scene, entt::entity entity, std::string_view group)
{
	if (!scene.GetRegistry().valid(entity))
		return EResult::InvalidArgument;

	auto& comp = scene.GetRegistry().emplace_or_replace<ScenePersistenceComponent>(entity);
	comp.persistAcrossSceneLoads = true;
	comp.persistentGroup = std::string(group);

	return EResult::Success;
}

EResult SceneManager::UnmarkDontDestroyOnLoad(Scene& scene, entt::entity entity)
{
	if (!scene.GetRegistry().valid(entity))
		return EResult::InvalidArgument;

	if (scene.GetRegistry().all_of<ScenePersistenceComponent>(entity))
	{
		scene.GetRegistry().remove<ScenePersistenceComponent>(entity);
	}

	return EResult::Success;
}

EResult SceneManager::TransferPersistentEntities(Scene& fromScene, Scene& toScene)
{
	if (&fromScene == &toScene)
		return EResult::Success;

	auto& fromRegistry = fromScene.GetRegistry();
	auto& toRegistry = toScene.GetRegistry();

	auto view = fromRegistry.view<ScenePersistenceComponent>();
	if (view.empty())
		return EResult::Success;

	std::vector<entt::entity> candidates;
	for (auto entity : view)
	{
		const auto& comp = view.get<ScenePersistenceComponent>(entity);
		if (comp.persistAcrossSceneLoads)
		{
			candidates.push_back(entity);
		}
	}

	if (candidates.empty())
		return EResult::Success;

	std::vector<entt::entity> persistentRoots;
	for (auto entity : candidates)
	{
		bool hasPersistentAncestor = false;
		if (const auto* hierarchy = fromRegistry.try_get<HierarchyComponent>(entity))
		{
			entt::entity parent = hierarchy->parent;
			while (parent != entt::null && fromRegistry.valid(parent))
			{
				if (const auto* parentPersist = fromRegistry.try_get<ScenePersistenceComponent>(parent))
				{
					if (parentPersist->persistAcrossSceneLoads)
					{
						hasPersistentAncestor = true;
						break;
					}
				}
				const auto* parentHierarchy = fromRegistry.try_get<HierarchyComponent>(parent);
				parent = parentHierarchy ? parentHierarchy->parent : entt::null;
			}
		}

		if (!hasPersistentAncestor)
		{
			persistentRoots.push_back(entity);
		}
	}

	for (auto rootHandle : persistentRoots)
	{
		Entity* fromEntity = fromScene.FindEntity(rootHandle);
		if (!fromEntity)
			continue;

		bool transferredViaPrefab = false;

		// 1. Prefab archive transfer
		archive::JsonArchive writer;
		auto writeRes = fromScene.WritePrefabArchive(writer, *fromEntity);
		if (writeRes)
		{
			std::string jsonStr;
			auto encRes = writer.ToJson(jsonStr);
			if (encRes)
			{
				archive::JsonArchive reader;
				auto decRes = reader.Parse(jsonStr);
				if (decRes)
				{
					Entity* outRootEntity = nullptr;
					auto instRes = toScene.InstantiatePrefabArchive(reader, outRootEntity, nullptr);
					if (instRes && outRootEntity)
					{
						transferredViaPrefab = true;

						const auto* origComp = fromRegistry.try_get<ScenePersistenceComponent>(rootHandle);
						if (origComp)
						{
							toRegistry.emplace_or_replace<ScenePersistenceComponent>(
								outRootEntity->GetEntityHandle(), *origComp);
						}
					}
				}
			}
		}

		// 2. Fallback to manual recursive transfer if reflection/prefab unavailable
		if (!transferredViaPrefab)
		{
			std::function<void(entt::entity, Entity*)> copySubtree = [&](entt::entity srcHandle, Entity* destParent)
			{
				Entity* newEntity = &toScene.CreateEntity();
				const auto newHandle = newEntity->GetEntityHandle();

				if (const auto* name = fromRegistry.try_get<NameComponent>(srcHandle))
					toRegistry.emplace_or_replace<NameComponent>(newHandle, *name);
				if (const auto* tag = fromRegistry.try_get<TagComponent>(srcHandle))
					toRegistry.emplace_or_replace<TagComponent>(newHandle, *tag);
				if (const auto* transform = fromRegistry.try_get<TransformComponent>(srcHandle))
					toRegistry.emplace_or_replace<TransformComponent>(newHandle, *transform);
				if (const auto* worldTransform = fromRegistry.try_get<WorldTransformComponent>(srcHandle))
					toRegistry.emplace_or_replace<WorldTransformComponent>(newHandle, *worldTransform);
				if (const auto* flag = fromRegistry.try_get<FlagComponent>(srcHandle))
					toRegistry.emplace_or_replace<FlagComponent>(newHandle, *flag);
				if (const auto* persist = fromRegistry.try_get<ScenePersistenceComponent>(srcHandle))
					toRegistry.emplace_or_replace<ScenePersistenceComponent>(newHandle, *persist);

				if (destParent)
				{
					static_cast<void>(toScene.ReparentEntity(*newEntity, destParent));
				}

				if (const auto* hier = fromRegistry.try_get<HierarchyComponent>(srcHandle))
				{
					for (auto childHandle : hier->children)
					{
						if (fromRegistry.valid(childHandle))
							copySubtree(childHandle, newEntity);
					}
				}
			};

			copySubtree(rootHandle, nullptr);
		}
	}

	return EResult::Success;
}
#pragma endregion

#pragma region Save & Load
EResult SceneManager::SaveScene(archive::IArchive& ar, const wstring& filePath)
{
	if (filePath.empty())
		return EResult::InvalidArgument;
	if (!m_CurrentScene)
		return EResult::Fail;
	std::filesystem::path targetPath(filePath);
	if (targetPath.is_relative())
		targetPath = PlatformPath::ResolveProjectRelative(targetPath);
	auto result = m_CurrentScene->WriteArchive(ar);
	if (result)
		result = ar.SaveToFile(targetPath);
	if (!result)
	{
		ENGINE_LOG_ERROR("Scene save failed: code={}, field={}, path={}", static_cast<int32>(result.Code), result.Field, WStrToStr(filePath));
		return EResult::Fail;
	}
	return EResult::Success;
}

EResult SceneManager::LoadScene(archive::IArchive& ar, const wstring& filePath)
{
	if (filePath.empty())
		return EResult::InvalidArgument;
	std::filesystem::path targetPath(filePath);
	if (targetPath.is_relative())
		targetPath = PlatformPath::ResolveProjectRelative(targetPath);
	if (!std::filesystem::exists(targetPath))
	{
		const std::string ext = targetPath.extension().string();
		if (ext == ".json")
		{
			const std::string stem = targetPath.stem().string();
			if (!stem.ends_with(".bamscene"))
			{
				const auto candidatePath = targetPath.parent_path() / (stem + ".bamscene.json");
				if (std::filesystem::exists(candidatePath))
					targetPath = candidatePath;
			}
		}
	}
	using Clock = std::chrono::steady_clock;
	const auto elapsedMs = [](Clock::time_point begin, Clock::time_point end) { return std::chrono::duration<double, std::milli>(end - begin).count(); };
	const auto readStart = Clock::now();
	Scene* candidate = nullptr;
	auto result = ar.LoadFromFile(targetPath);
	const auto createStart = Clock::now();
	if (result) result = Scene::CreateFromArchive(ar, candidate);
	const auto openStart = Clock::now();
	if (!result)
	{
		ENGINE_LOG_ERROR("Scene load failed: code={}, field={}, path={}", static_cast<int32>(result.Code), result.Field, WStrToStr(filePath));
		return EResult::Fail;
	}
	const EResult openResult = OpenScene(candidate);
	Safe_Release(candidate);
	ENGINE_LOG_INFO("Scene load timing: {} | archive {:.0f} ms, resources and entities {:.0f} ms, open {:.0f} ms",
		WStrToStr(targetPath.filename().wstring()), elapsedMs(readStart, createStart), elapsedMs(createStart, openStart),
		elapsedMs(openStart, Clock::now()));
	return openResult;
}
#pragma endregion

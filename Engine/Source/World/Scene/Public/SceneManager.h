#pragma once

#include "Scene.h"
#include "ScenePersistenceComponent.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>

BEGIN(Engine)
class ENGINE_API SceneManager : public Base
{
#pragma region Constructor&Destructor
	DECLARE_SINGLETON(SceneManager)
protected:
	SceneManager() {}
	virtual ~SceneManager() {}
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Scene Management
public:
	[[nodiscard]] Scene* GetCurrentScene() const { return m_CurrentScene; }
	[[nodiscard]] vector<Scene*> GetActiveScenes();
	[[nodiscard]] vector<Scene*> GetAllScenes() const { return m_Scenes; }
public:
	EResult AddScene(Scene* scene);
	EResult RemoveScene(Scene* scene);
	EResult OpenScene(Scene* newScene);
	EResult ChangeScene(Scene* newScene);
	EResult ChangeScene(archive::IArchive& ar, const wstring& filePath);
	EResult CloseScene();
	EResult NewScene(void* arg = nullptr);

	EResult SavePrefab(archive::IArchive& ar, const wstring& filePath, const Entity& rootEntity);
	EResult InstantiatePrefab(archive::IArchive& ar, const wstring& filePath, Entity*& outRootEntity, Entity* parent = nullptr);
#pragma endregion

#pragma region Additive SubScene Streaming
public:
	EResult LoadSceneAdditive(const std::filesystem::path& scenePath, const std::string& subSceneName);
	EResult UnloadAdditiveScene(const std::string& subSceneName);
	EResult UnloadAllAdditiveScenes();
	[[nodiscard]] Scene* GetAdditiveScene(const std::string& subSceneName);
	[[nodiscard]] std::vector<std::string> GetAdditiveSceneNames() const;
	EResult RegisterAdditiveScene(const std::string& subSceneName, Scene* scene);
#pragma endregion

#pragma region Persistent Entities
public:
	EResult MarkDontDestroyOnLoad(Scene& scene, entt::entity entity, std::string_view group = "Global");
	EResult UnmarkDontDestroyOnLoad(Scene& scene, entt::entity entity);
	EResult TransferPersistentEntities(Scene& fromScene, Scene& toScene);
#pragma endregion

#pragma region Save & Load
public:
	EResult SaveScene(archive::IArchive& ar, const wstring& filePath);
	EResult LoadScene(archive::IArchive& ar, const wstring& filePath);
#pragma endregion

#pragma region Variable
protected:
	Scene* m_CurrentScene = { nullptr };
	vector<Scene*> m_Scenes;
	vector<Scene*> m_ActiveScenes;
	std::unordered_map<std::string, Scene*> m_AdditiveScenes;
#pragma endregion

};
END

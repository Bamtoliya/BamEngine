#pragma once

#include "Scene.h"

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
	Scene* GetCurrentScene() const { return m_CurrentScene; }
	vector<Scene*> GetActiveScenes();
	vector<Scene*> GetAllScenes() const { return m_Scenes; }
public:
	EResult AddScene(Scene* scene);
	EResult RemoveScene(Scene* scene);
	EResult OpenScene(Scene* newScene);
	EResult CloseScene();
	EResult NewScene(void* arg = nullptr);

	EResult SavePrefab(archive::IArchive& ar, const wstring& filePath, const Entity& rootEntity);
	EResult InstantiatePrefab(archive::IArchive& ar, const wstring& filePath, Entity*& outRootEntity, Entity* parent = nullptr);
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
#pragma endregion

};
END

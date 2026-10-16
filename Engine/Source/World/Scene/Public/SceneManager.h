#pragma once

#include "Scene.h"

BEGIN(Engine)
enum class ESceneState : uint8
{
	Edit,
	Play,
	Pause,
};

class ENGINE_API SceneManager : public Base
{
	DECLARE_SINGLETON(SceneManager)
#pragma region Constructor&Destructor
protected:
	SceneManager() {}
	virtual ~SceneManager() {}
	EResult Initialize(void* arg = nullptr);
public:
	static bool IsCreated() noexcept { return m_instance != nullptr; }
	virtual void Free() override;
#pragma endregion

//#pragma region Loop
//public:
//	void FixedUpdate(f32 dt);
//	void Update(f32 dt);
//	void LateUpdate(f32 dt);
//#pragma endregion

#pragma region Scene Management
public:
	EResult OpenScene(Scene* newScene);
	EResult CloseScene();
	Scene* GetCurrentScene() const { return m_CurrentScene; }

	EResult NewScene(void* arg = nullptr);
	EResult SaveScene(archive::IArchive& ar, const wstring& filePath);
	EResult LoadScene(archive::IArchive& ar, const wstring& filePath);
	EResult SavePrefab(archive::IArchive& ar, const wstring& filePath, const Entity& rootEntity);
	EResult InstantiatePrefab(archive::IArchive& ar, const wstring& filePath,
		Entity*& outRootEntity, Entity* parent = nullptr);

	EResult AddScene(Scene* scene);
	EResult RemoveScene(Scene* scene);

	vector<Scene*> GetActiveScenes();
	vector<Scene*> GetAllScenes() const { return m_Scenes; }
#pragma endregion

#pragma region Variable
protected:
	Scene* m_CurrentScene = { nullptr };
	vector<Scene*> m_Scenes;
	vector<Scene*> m_ActiveScenes;
#pragma endregion

};
END
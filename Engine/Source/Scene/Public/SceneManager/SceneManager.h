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
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	void FixedUpdate(f32 dt);
	void Update(f32 dt);
	void LateUpdate(f32 dt);
#pragma endregion

#pragma region Scene Management
public:
	EResult OpenScene(Scene* newScene);
	class Scene* GetCurrentScene() const { return m_CurrentScene; }

	EResult NewScene(void* arg = nullptr);
	EResult SaveScene(const wstring& filePath);
	EResult LoadScene(const wstring& filePath);
#pragma endregion

#pragma region Variable
protected:
	Scene* m_CurrentScene = { nullptr };
	vector<Scene*> m_Scenes;
#pragma endregion

};
END
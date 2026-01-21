#pragma once

#include "Scene.h"

BEGIN(Engine)
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
	EResult Render(f32 dt);
#pragma endregion


#pragma region Scene Management
public:
	EResult OpenScene(Scene* newScene);
	class Scene* GetActiveScene() const { return m_ActiveScene; }
#pragma endregion



#pragma region Variable
protected:
	Scene* m_ActiveScene = { nullptr };
	vector<Scene*> m_Scenes;
#pragma endregion

};
END
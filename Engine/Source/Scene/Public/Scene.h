#pragma once

#include "Base.h"

BEGIN(Engine)
class ENGINE_API Scene : public Base
{
#pragma region Constructor&Destructor
protected:
	Scene() {}
	virtual ~Scene() {}
	virtual EResult Initialize(void* arg = nullptr);
public:
	static Scene* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	virtual void FixedUpdate(f32 dt);
	virtual void Update(f32 dt);
	virtual void LateUpdate(f32 dt);
#pragma endregion


#pragma region Object Management
public:
	EResult AddGameObject(class GameObject* gameObject, uint32 layerIndex);

	EResult RemoveGameObject(class GameObject* gameObject, uint32 layerIndex);
	EResult RemoveGameObject(class GameObject* gameObject);

	class Layer* FindLayerByIndex(uint32 layerIndex) const;
	class Layer* FindLayerByName(const wstring& layerName) const;

	const vector<class Layer*>& GetAllLayers() const { return m_Layers; }
#pragma endregion

#pragma region Variable
protected:
	wstring m_SceneName = L"Scene";
	vector<class Layer*> m_Layers;
#pragma endregion
};
END
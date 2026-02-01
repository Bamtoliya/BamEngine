#pragma once

#include "Base.h"

BEGIN(Engine)

struct tagSceneCreateDesc
{
	wstring name = L"Scene";
};

enum class ESceneFlags : uint8
{
	None = 0,
	Active = 1 << 0,
};

ENABLE_BITMASK_OPERATORS(ESceneFlags)

class ENGINE_API Scene : public Base
{
#pragma region Constructor&Destructor
protected:
	using DESC = tagSceneCreateDesc;
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


#pragma region Scene Management
public:
	void SetName(const wstring& name) { m_Name = name; }
	const wstring& GetName() const { return m_Name; }
#pragma endregion

#pragma region Layer Management
public:
	EResult AddLayer(class Layer* layer);
	EResult InsertLayer(uint32 layerIndex, class Layer* layer);
	EResult CreateLayer(const wstring& layerName, uint32 layerIndex = -1);

	EResult RemoveLayer(uint32 layerIndex);
	EResult RemoveLayer(class Layer* layer);
	EResult RemoveLayer(const wstring& layerName);

	const vector<class Layer*>& GetAllLayers() const { return m_Layers; }

	class Layer* FindLayer(uint32 layerIndex) const;
	class Layer* FindLayer(const wstring& layerName) const;

	EResult ReorderLayer(uint32 oldIndex, uint32 newIndex);
	void SetLayerName(uint32 layerIndex, const wstring& name);

private:
	void UpdateLayerIndices(uint32 startIndex);
#pragma endregion

#pragma region Object Management
public:
	EResult AddGameObject(class GameObject* gameObject, uint32 layerIndex = -1);
	EResult RemoveGameObject(class GameObject* gameObject);

	class GameObject* FindGameObject(const wstring& name, uint32 layerIndex = -1);
	class GameObject* FindGameObject(uint64 id, uint32 layerIndex = -1);

	EResult MoveGameObjectLayer(class GameObject* gameObject, uint32 targetLayerIndex);
#pragma endregion

#pragma region Flag Mangement
public:
	void SetActive(bool active);
#pragma endregion


#pragma region Variable
protected:
	ESceneFlags m_Flags = ESceneFlags::Active;
	wstring m_Name = { L"Scene" };
	vector<class Layer*> m_Layers;
#pragma endregion
};
END
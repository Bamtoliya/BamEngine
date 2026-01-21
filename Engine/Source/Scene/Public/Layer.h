#pragma once

#include "Base.h"

BEGIN(Engine)

enum class ELayerFlags : uint8
{
	None = 0,
	Active = 1 << 0,
	Visible = 1 << 1,
	Default = Active | Visible
};

ENABLE_BITMASK_OPERATORS(ELayerFlags)
 
class ENGINE_API Layer : public Base
{
#pragma region Struct
public:
	typedef struct tagLayerCreateArg
	{
		uint32 layerIndex = 0;
		wstring layerName = L"Layer";
	} LAYERDESC;
#pragma endregion

#pragma region Constructor&Destructor
protected:
	Layer() {}
	virtual ~Layer() {}
	EResult Initialize(void* arg = nullptr);
public:
	static Layer* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	void FixedUpdate(f32 dt);
	void Update(f32 dt);
	void LateUpdate(f32 dt);
#pragma endregion

#pragma region Layer Management
public:
	uint32 GetLayerIndex() const { return m_LayerIndex; }
	wstring GetLayerName() const { return m_LayerName; }
public:
	void SetLayerIndex(uint32 index) { m_LayerIndex = index; }
	void SetLayerName(const wstring& name) { m_LayerName = name; }
#pragma endregion

#pragma region Object Management
public:
	EResult AddGameObject(class GameObject* gameObject);
	EResult RemoveGameObject(class GameObject* gameObject);
public:
	class GameObject* FindGameObjectByID(uint64 id) const;
	class GameObject* FindGameObjectByName(const wstring& name) const;
public:
	vector<class GameObject*> GetAllGameObjects() const;
	vector<class GameObject*> FindGameObjectsByTag(const wstring& tag) const;
#pragma endregion

#pragma region Flag Management
public:
	void SetVisible(bool visible) { SetFlag(m_Flags, ELayerFlags::Visible, visible); }
	void SetActive(bool active) { SetFlag(m_Flags, ELayerFlags::Active, active); }
public:
	bool IsVisible() const { return HasFlag(m_Flags, ELayerFlags::Visible); }
	bool IsActive()  const { return HasFlag(m_Flags, ELayerFlags::Active); }
public:
	ELayerFlags GetFlags() const { return m_Flags; }
	void SetFlags(ELayerFlags flags) { m_Flags = flags; }
#pragma endregion


#pragma region Variable
protected:
	uint32 m_LayerIndex = { 0 };
	wstring m_LayerName = L"";
	ELayerFlags m_Flags = ELayerFlags::Default;
	vector<class GameObject*> m_GameObjects;
#pragma endregion


};
END
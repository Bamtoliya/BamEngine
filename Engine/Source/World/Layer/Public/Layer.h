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

#pragma region Struct
struct tagLayerCreateDesc
{
	uint32 index = 0;
	wstring name = L"Layer";
};
#pragma endregion
 
class ENGINE_API Layer : public Base
{
#pragma region Constructor&Destructor
protected:
	using DESC = tagLayerCreateDesc;
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
	uint32 GetIndex() const { return m_Index; }
	wstring GetName() const { return m_Name; }
public:
	void SetIndex(uint32 index);
	void SetName(const wstring& name) { m_Name = name; }
#pragma endregion

#pragma region Object Management
public:
	EResult AddGameObject(class GameObject* gameObject);
	EResult RemoveGameObject(class GameObject* gameObject);
	EResult MoveGameObject(class GameObject* gameObject, int8 dir);
	EResult CloneGameObject(class GameObject* gameObject);
public:
	class GameObject* FindGameObject(class GameObject* gameObject) const;
	class GameObject* FindGameObject(uint64 id) const;
	class GameObject* FindGameObject(const wstring& name) const;
public:
	vector<class GameObject*> GetAllGameObjects() const { return m_GameObjects; }
	vector<class GameObject*> FindGameObjectsByTag(const wstring& tag) const;
public:
	EResult RemoveDeadGameObject(class GameObject* gameObject);
#pragma endregion

#pragma region Flag Management
public:
	void SetVisible(bool visible);
	void SetActive(bool active);
public:
	void SetAllObjectVisible(bool visible);
	void SetAllObjectActive(bool active);
public:
	bool IsVisible() const { return HasFlag(m_Flags, ELayerFlags::Visible); }
	bool IsActive()  const { return HasFlag(m_Flags, ELayerFlags::Active); }
public:
	ELayerFlags GetFlags() const { return m_Flags; }
	void SetFlags(ELayerFlags flags) { m_Flags = flags; }
public:
#pragma endregion


#pragma region Variable
protected:
	uint32 m_Index = { 0 };
	wstring m_Name = L"Layer";
	ELayerFlags m_Flags = ELayerFlags::Default;
	vector<class GameObject*> m_GameObjects;
	vector<class GameObject*> m_DeadGameObjects;
#pragma endregion


};
END
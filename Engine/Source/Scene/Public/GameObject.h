#pragma once

#include "Base.h"
#include "Component.h"
#include "ReflectionMacro.h"
#include "ComponentRegistry.h"

ENUM()
enum class EObjectFlag : uint8
{
	None = 0,
	Active = 1 << 0,
	Visible = 1 << 1,
	Paused = 1 << 2,
	Dead = 1 << 3,
	Default = Active | Visible,
};

ENABLE_BITMASK_OPERATORS(EObjectFlag)

BEGIN(Engine)

#pragma region Struct
struct tagGameObjectDesc
{
	wstring name = L"GameObject";
};
#pragma endregion

CLASS()
class ENGINE_API GameObject : public Base
{
	REFLECT_CLASS(GameObject)

	using DESC = tagGameObjectDesc;
#pragma region Constructor&Destructor
protected:
	GameObject() {}
	virtual ~GameObject() {}
	virtual EResult Initialize(void* arg = nullptr);
public:
	static GameObject* Create(void* arg = nullptr);
	virtual GameObject* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Loop
	virtual void	FixedUpdate(f32 dt);
	virtual void	Update(f32 dt);
	virtual void	LateUpdate(f32 dt);
#pragma endregion

#pragma region Component Management
public:
	EResult RemoveComponent(const wstring& tag);
	EResult RemoveComponent(Component* component);

	template<typename T>
	T* AddComponent(void* arg = nullptr)
	{
		T* component = ComponentRegistry::Get().Create<T>(arg);
		if (!component)
			return nullptr;

		AttachComponent(component);
		Safe_Release(component);

		return component;
	}
	
	Component* AddComponent(const wstring& componentTag, void* arg = nullptr)
	{
		Component* component = ComponentRegistry::Get().Create(componentTag, arg);
		if (!component)
			return nullptr;

		AttachComponent(component);
		Safe_Release(component);

		return component;
	}

	EResult AttachComponent(Component* component);

	template<typename T>
	T* GetComponent()
	{
		for (Component* comp : m_Components)
		{
			T* castedComp = dynamic_cast<T*>(comp);
			if (castedComp)
			{
				return castedComp;
			}
		}
		return nullptr;
	}
	template<typename T>
	T* GetComponent(const wstring& tag)
	{
		for (Component* comp : m_Components)
		{
			if (comp->GetTag() == tag)
			{
				return dynamic_cast<T*>(comp);
			}
		}
		return nullptr;
	}
	template<typename T>
	vector<T*> GetComponents()
	{
		vector<T*> components;
		for (Component* comp : m_Components)
		{
			T* castedComp = dynamic_cast<T*>(comp);
			if (castedComp)
			{
				components.push_back(castedComp);
			}
		}
		return components;
	}
	Component* GetComponent(const wstring& tag);
	vector<Component*> GetAllComponents() const { return m_Components; }
#pragma endregion

#pragma region Parent & Child
public:
	GameObject* GetParent() const { if (m_Parent) return m_Parent; return nullptr; }
	EResult SetParent(GameObject* parent) { if (!parent) return EResult::Fail; m_Parent = parent; return EResult::Success; }
	EResult ClearParent() { if (m_Parent) m_Parent = nullptr; return EResult::Success; }
public:
	bool IsDescendant(GameObject* target) const;
public:
	vector<GameObject*> GetAllChilds() const { return m_Childs; }
	EResult AddChild(GameObject* child);
	EResult RemoveChild(GameObject* child);
	EResult MoveChild(GameObject* child, int8 dir);
private:
	void UpdateChildIndex();
#pragma endregion

#pragma region ID
public:
	uint64 GetID() const { return m_ID; }
#pragma endregion

#pragma region Tag Management
public:
	EResult AddTag(const wstring& tag);
	EResult RemoveTag(const wstring& tag);
	bool HasTag(const wstring& tag) const;
#pragma endregion

#pragma region Name Management
public:
	EResult SetName(const wstring& name) { m_Name = name; return EResult::Success; }
	wstring GetName() const { return m_Name; }
#pragma endregion

#pragma region Flag Management
public:
	EObjectFlag GetFlags() const { return m_Flags; }
	bool IsActive() const { return HasFlag(m_Flags, EObjectFlag::Active); }
	bool IsVisible() const { return HasFlag(m_Flags, EObjectFlag::Visible); }
	bool IsPaused() const { return HasFlag(m_Flags, EObjectFlag::Paused); }
	bool IsDead() const { return HasFlag(m_Flags, EObjectFlag::Dead); }
	EResult SetFlags(EObjectFlag flags) { m_Flags = flags; return EResult::Success; }
public:
	void SetVisible(bool visible);
	void SetActive(bool active);
	void SetPaused(bool paused);
	void SetDead(bool dead = true);
public:
	void SetAllChildVisible(bool visible);
	void SetAllChildActive(bool active);
	bool IsVisibleInHierarchy() const;
#pragma endregion

#pragma region Layer Management
public:
	uint32 GetIndex() const { return m_Index; }
	void SetIndex(uint32 index);
	uint32 GetLayerIndex() const { return m_LayerIndex; }
	void SetLayerIndex(uint32 layerIndex);
#pragma endregion



#pragma region Variables
protected:

	GameObject* m_Parent = { nullptr };

	vector<GameObject*> m_Childs = {};

	vector<Component*> m_Components = {};

	PROPERTY(CATEGORY("PROP_INFORMATION"), READONLY)
	uint64 m_ID = { 0 };

	PROPERTY(CATEGORY("PROP_INFORMATION"), READONLY)
	uint32 m_Index = { 0 };

	PROPERTY(CATEGORY("PROP_INFORMATION"), READONLY)
	uint32 m_LayerIndex = { static_cast<uint32>(-1) };

	PROPERTY("PROP_TAGS")
	unordered_set<wstring> m_TagSet = {};

	PROPERTY("PROP_NAME")
	wstring m_Name = { L"GameObject" };

	PROPERTY("PROP_BITFLAG")
	EObjectFlag m_Flags = { EObjectFlag::Default };
#pragma endregion

};
END
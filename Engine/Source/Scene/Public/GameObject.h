#pragma once

#include "Base.h"
#include "Component.h"
#include "ReflectionMacro.h"
#include "ComponentRegistry.h"

enum class EObjectFlag : uint8
{
	None = 0,
	Active = 1 << 0,
	Visible = 1 << 1,
	Paused = 1 << 2,
	Dead = 1 << 3,
	Default = Active | Visible
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
	EResult AddComponent(Component* component);
	EResult AddComponent(Component* component, const wstring& tag);
	EResult RemoveComponent(const wstring& tag);
	EResult RemoveComponent(Component* component);

	template<typename T>
	EResult AddComponent(void* arg = nullptr)
	{
		Component* component = ComponentRegistry::Get().Create<T>(arg);
		if (!component)
			return EResult::Fail;

		return AddComponent(component);
	}
		
	EResult AddComponent(const wstring& componentTag, void* arg = nullptr)
	{
		Component* component = ComponentRegistry::Get().Create(componentTag, arg);
		if (!component)
			return EResult::Fail;

		return AddComponent(component);
	}

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
#pragma endregion

#pragma region Parent
public:
	GameObject* GetParent() const { if (m_Parent) return m_Parent; return nullptr; }
	EResult SetParent(GameObject* parent) { if (!parent) return EResult::Fail; m_Parent = parent; return EResult::Success; }
#pragma endregion

#pragma region ID
public:
	uint64 GetID() const { return m_ID; }
#pragma endregion

#pragma region Child Management	
public:
	EResult AddChild(GameObject* child);
	EResult RemoveChild();
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
	void SetActive(bool active)
	{
		if (active)
			AddFlag(m_Flags, EObjectFlag::Active);
		else
			m_Flags &= ~EObjectFlag::Active;
	}
#pragma endregion


#pragma region Variables
protected:

	PROPERTY()
	GameObject* m_Parent = { nullptr };

	PROPERTY()
	vector<GameObject*> m_Childs = {};

	PROPERTY()
	vector<Component*> m_Components = {};

	PROPERTY()
	unordered_set<wstring> m_TagSet = {};

	PROPERTY()
	wstring m_Name = { L"GameObject" };

	PROPERTY()
	uint64 m_ID = { 0 };

	PROPERTY()
	EObjectFlag m_Flags = { EObjectFlag::Default };
#pragma endregion

};
END
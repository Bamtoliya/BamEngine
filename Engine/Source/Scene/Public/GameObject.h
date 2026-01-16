#pragma once

#include "Base.h"
#include "Component.h"

enum class EObjectFlag : uint8
{
	None = 0,
	Active = 1 << 0,
	Visible = 1 << 1,
	Paused = 1 << 2,
	Dead = 1 << 3,
};

BEGIN(Engine)
class ENGINE_API GameObject : public Base
{
#pragma region Struct
public:
	typedef struct GameObjectCreateArg
	{
		wstring name = L"GameObject";
	} GAMEOBJECTDESC;
#pragma endregion

#pragma region Constructor&Destructor
protected:
	GameObject() {}
	virtual ~GameObject() {}
	virtual EResult Initialize(void* arg = nullptr);
public:
	static GameObject* Create(void* arg = nullptr);
	virtual GameObject* Clone(void* arg = nullptr) { return nullptr; }
	virtual void Free() override;
#pragma endregion

#pragma region Loop
	virtual void	FixedUpdate(f32 dt) {};
	virtual void	Update(f32 dt) {};
	virtual void	LateUpdate(f32 dt) {};

	virtual EResult	Render(f32 dt) { return EResult::Success; }
#pragma endregion

#pragma region Component Management
public:
	EResult AddComponent(Component* component);
	EResult RemoveComponent();

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
#pragma endregion

#pragma region Parent
	GameObject* GetParent() const { if (m_Parent) return m_Parent; return nullptr; }
	EResult SetParent(GameObject* parent) { if (!parent) return EResult::Fail; m_Parent = parent; return EResult::Success; }
#pragma endregion

#pragma region Child Management	
	EResult AddChild(GameObject* child);
	EResult RemoveChild();
#pragma endregion

#pragma region Tag Management
	EResult AddTag(const wstring& tag);
	EResult RemoveTag(const wstring& tag);
	bool HasTag(const wstring& tag) const;
#pragma endregion

#pragma region Name Management
	EResult SetName(const wstring& name) { m_Name = name; return EResult::Success; }
	wstring GetName() const { return m_Name; }
#pragma endregion

#pragma region Variables
protected:
	GameObject* m_Parent = { nullptr };
	vector<GameObject*> m_Childs = {};
	vector<Component*> m_Components = {};
	unordered_set<wstring> m_TagSet = {};
	wstring m_Name = { L"" };
	uint64 m_ID = { 0 };
	bool m_Active = { true };
	bool m_Visible = { true };
#pragma endregion

};
END
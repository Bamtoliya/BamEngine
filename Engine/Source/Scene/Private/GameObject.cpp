#pragma once

#include "GameObject.h"

static uint64 g_GameObjectIDCounter = 0;

#pragma region Constructor&Destructor
EResult GameObject::Initialize(void* arg)
{
	GAMEOBJECTDESC* desc = static_cast<GAMEOBJECTDESC*>(arg);
	if (desc)
	{
		m_Name = desc->name;
	}
	m_ID = ++g_GameObjectIDCounter;
	return EResult::Success;
}
GameObject* GameObject::Create(void* arg)
{
	GameObject* instance = new GameObject();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void GameObject::Free()
{
	for (Component* comp : m_Components)
	{
		Safe_Release(comp);
	}
	m_Components.clear();
	for (GameObject* child : m_Childs)
	{
		Safe_Release(child);
	}
	m_Childs.clear();
}

#pragma endregion


#pragma region Loop
void GameObject::FixedUpdate(f32 dt)
{
	if (!m_Active) return;
	for(Component* comp : m_Components)
	{
		comp->FixedUpdate(dt);
	}
}
void GameObject::Update(f32 dt)
{
	if (!m_Active) return;
	for (Component* comp : m_Components)
	{
		comp->Update(dt);
	}
}
void GameObject::LateUpdate(f32 dt)
{
	if (!m_Active) return;
	for (Component* comp : m_Components)
	{
		comp->LateUpdate(dt);
	}
}
EResult GameObject::Render(f32 dt)
{
	if (!m_Active || !m_Visible) return EResult::Success;
	for (Component* comp : m_Components)
	{
		if (IsFailure(comp->Render(dt)))
			return EResult::Fail;
	}
	return EResult::Success;
}
#pragma endregion


#pragma region Component Management

EResult GameObject::AddComponent(Component* component)
{
	if (!component)
		return EResult::Fail;
	Safe_AddRef(component);
	m_Components.push_back(component);
	return EResult::Success;
}

EResult GameObject::RemoveComponent()
{
	if (m_Components.empty())
		return EResult::Fail;
	Component* comp = m_Components.back();
	comp->Free();
	Safe_Release(comp);
	m_Components.pop_back();
	return EResult::Success;
}
#pragma endregion

#pragma region Child Management
EResult GameObject::AddChild(GameObject* child)
{
	if (!child)
		return EResult::Fail;
	m_Childs.push_back(child);
	return EResult::Success;
}
EResult GameObject::RemoveChild()
{
	if (m_Childs.empty())
		return EResult::Fail;
	GameObject* child = m_Childs.back();
	child->Free();
	Safe_Release(child);
	m_Childs.pop_back();
	return EResult::Success;
}
#pragma endregion

#pragma region Tag Management
EResult GameObject::AddTag(const wstring& tag)
{
	if (m_TagSet.find(tag) != m_TagSet.end())
		return EResult::Fail;
	m_TagSet.insert(tag);
	return EResult::Success;
}
EResult GameObject::RemoveTag(const wstring& tag)
{
	if (m_TagSet.find(tag) == m_TagSet.end())
		return EResult::Fail;
	m_TagSet.erase(tag);
	return EResult::Success;
}
bool GameObject::HasTag(const wstring& tag) const
{
	return m_TagSet.find(tag) != m_TagSet.end();
}
#pragma endregion


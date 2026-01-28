#pragma once

#include "GameObject.h"

static uint64 g_GameObjectIDCounter = 0;

#pragma region Constructor&Destructor
EResult GameObject::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
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

GameObject* GameObject::Clone(void* arg)
{
	GameObject* instance = new GameObject(*this);
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}

	for (Component* comp : m_Components)
	{
		Component* clonedComp = comp->Clone(instance, arg);
		if (IsFailure(instance->AddComponent(clonedComp)))
		{
			Safe_Release(clonedComp);
			Safe_Release(instance);
			return nullptr;
		}
		Safe_Release(clonedComp);
	}

	return instance;
}

void GameObject::Free()
{
	RELEASE_VECTOR(m_Components);
	RELEASE_VECTOR(m_Childs);
}

#pragma endregion

#pragma region Loop
void GameObject::FixedUpdate(f32 dt)
{
	if (!IsActive()) return;
	for(Component* comp : m_Components)
	{
		comp->FixedUpdate(dt);
	}
}
void GameObject::Update(f32 dt)
{
	if (!IsActive()) return;
	for (Component* comp : m_Components)
	{
		comp->Update(dt);
	}
}
void GameObject::LateUpdate(f32 dt)
{
	if (!IsActive()) return;
	for (Component* comp : m_Components)
	{
		comp->LateUpdate(dt);
	}
}
#pragma endregion

#pragma region Component Management

EResult GameObject::AddComponent(Component* component)
{
	if (!component)
		return EResult::Fail;
	component->SetOwner(this);
	Safe_AddRef(component);
	m_Components.push_back(component);
	return EResult::Success;
}

EResult GameObject::AddComponent(Component* component, const wstring& tag)
{
	if (!component)
		return EResult::Fail;
	component->SetOwner(this);
	component->SetTag(tag);
	Safe_AddRef(component);
	m_Components.push_back(component);
	return EResult::Success;
}

EResult GameObject::RemoveComponent(const wstring& tag)
{
	for (auto it = m_Components.begin(); it != m_Components.end(); ++it)
	{
		if ((*it)->GetTag() == tag)
		{
			(*it)->Free();
			Safe_Release(*it);
			m_Components.erase(it);
			return EResult::Success;
		}
	}
	return EResult::Fail;
}

EResult GameObject::RemoveComponent(Component* component)
{
	if (!component)
		return EResult::Fail;
	auto it = std::find(m_Components.begin(), m_Components.end(), component);
	if (it == m_Components.end())
		return EResult::Fail;
	(*it)->Free();
	Safe_Release(*it);
	m_Components.erase(it);
	return EResult::Success;
}
Component* GameObject::GetComponent(const wstring& tag)
{
	for (Component* comp : m_Components)
	{
		if (comp->GetTag() == tag)
		{
			return comp;
		}
	}
	return nullptr;
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


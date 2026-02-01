#pragma once

#include "GameObject.h"

static uint64 g_GameObjectIDCounter = 1;

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
	instance->m_Components.clear();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}

	for (Component* comp : m_Components)
	{
		Component* clonedComp = comp->Clone(instance, arg);
		if (IsFailure(instance->AttachComponent(clonedComp)))
		{
			Safe_Release(clonedComp);
			Safe_Release(instance);
			return nullptr;
		}
		Safe_Release(clonedComp);
	}

	for (auto& child : m_Childs)
	{
		GameObject* clonedChild = child->Clone(arg);
		if (IsFailure(instance->AddChild(clonedChild)))
		{
			Safe_Release(clonedChild);
			Safe_Release(instance);
			return nullptr;
		}
		Safe_Release(clonedChild);
		clonedChild->SetParent(instance);
	}

	return instance;
}

void GameObject::Free()
{
	RELEASE_VECTOR(m_Components);
	RELEASE_VECTOR(m_Childs);
	m_ID = -1;
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

EResult GameObject::AttachComponent(Component* component)
{
	if (!component)
		return EResult::Fail;
	component->SetOwner(this);
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
EResult GameObject::RemoveChild(GameObject* child)
{
	if (!child)
		return EResult::Fail;
	auto it = std::find(m_Childs.begin(), m_Childs.end(), child);
	if (it == m_Childs.end())
		return EResult::Fail;
	(*it)->Free();
	Safe_Release(*it);
	m_Childs.erase(it);
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

#pragma region Flag Management
void GameObject::SetVisible(bool visible)
{
	if (visible)
	{
		AddFlag(m_Flags, EObjectFlag::Visible);
	}
	else
	{
		RemoveFlag(m_Flags, EObjectFlag::Visible);
	}
	for (auto& component : m_Components)
	{
		if(RenderComponent* renderComp = dynamic_cast<RenderComponent*>(component))
		{
			renderComp->SetActive(visible);
		}
	}
	for (auto& child : m_Childs)
	{
		child->SetVisible(visible);
	}
}
void GameObject::SetActive(bool active)
{
	if (active)
	{
		AddFlag(m_Flags, EObjectFlag::Active);
	}
	else
	{
		RemoveFlag(m_Flags, EObjectFlag::Active);
	}
	for (auto& component : m_Components)
	{
		component->SetActive(active);
	}
	for (auto& child : m_Childs)
	{
		child->SetActive(active);
	}
}

#pragma endregion


#pragma region Layer Management
void GameObject::SetLayerIndex(uint32 layerIndex)
{
	m_LayerIndex = layerIndex;
	for(auto& child : m_Childs)
	{
		child->SetLayerIndex(layerIndex);
	}
}
#pragma endregion

#pragma once

#include "Layer.h"
#include "GameObject.h"

#pragma region Constructor&Destructor
EResult Layer::Initialize(void* arg)
{
    if (arg)
    {
        CAST_DESC
		m_Index = desc->index;
		m_Name = desc->name;
    }
    return EResult::Success;
}

Layer* Layer::Create(void* arg)
{
    Layer* instance = new Layer();
    if (IsFailure(instance->Initialize(arg)))
    {
        Safe_Release(instance);
        return nullptr;
    }
    return instance;
}

void Layer::Free()
{
	RELEASE_VECTOR(m_GameObjects);
}
#pragma endregion

#pragma region Loop
void Layer::FixedUpdate(f32 dt)
{
    if (!IsActive()) return;
	for (auto& gameObject : m_GameObjects)
    {
        gameObject->FixedUpdate(dt);
    }
}

void Layer::Update(f32 dt)
{
    if (!IsActive()) return;
    for (auto& gameObject : m_GameObjects)
    {
        gameObject->Update(dt);
    }
}

void Layer::LateUpdate(f32 dt)
{
    if (!IsActive()) return;
    for (auto& gameObject : m_GameObjects)
    {
        gameObject->LateUpdate(dt);
    }
}
#pragma endregion

#pragma region Layer Management
void Layer::SetIndex(uint32 index)
{
    m_Index = index;
    for(auto& gameObject : m_GameObjects)
    {
		gameObject->SetLayerIndex(index);
	}
}
#pragma endregion


#pragma region Object Management
EResult Layer::AddGameObject(GameObject* gameObject)
{
    if (!gameObject)
        return EResult::Fail;
    Safe_AddRef(gameObject);
    m_GameObjects.push_back(gameObject);
	gameObject->SetLayerIndex(m_Index);
    return EResult::Success;
}

EResult Layer::RemoveGameObject(GameObject* gameObject)
{
    if (!gameObject)
        return EResult::Fail;
    auto it = find(m_GameObjects.begin(), m_GameObjects.end(), gameObject);
    if (it != m_GameObjects.end())
    {
        Safe_Release(*it);
        m_GameObjects.erase(it);
    }

    return EResult::Success;
}

GameObject* Layer::FindGameObject(uint64 id) const
{
    for (GameObject* gameObject : m_GameObjects)
    {
        if (gameObject->GetID() == id)
            return gameObject;
    }
    return nullptr;
}

GameObject* Layer::FindGameObject(const wstring& name) const
{
    for (GameObject* gameObject : m_GameObjects)
    {
        if (gameObject->GetName() == name)
            return gameObject;
    }
    return nullptr;
}

vector<class GameObject*> Layer::FindGameObjectsByTag(const wstring& tag) const
{
    vector<class GameObject*> result;
    for (GameObject* gameObject : m_GameObjects)
    {
        if (gameObject->HasTag(tag))
            result.push_back(gameObject);
    }
    return result;
}
#pragma endregion

#pragma region Flag Managment
void Layer::SetVisible(bool visible)
{
    if (visible)
    {
		AddFlag(m_Flags, ELayerFlags::Visible);
    }
    else
    {
		RemoveFlag(m_Flags, ELayerFlags::Visible);
		SetAllObjectVisible(visible);
    }
}

void Layer::SetActive(bool active)
{
    if (active)
    {
        AddFlag(m_Flags, ELayerFlags::Active);
    }
    else
    {
        RemoveFlag(m_Flags, ELayerFlags::Active);
		SetAllObjectActive(active);
    }
}

void Layer::SetAllObjectVisible(bool visible)
{
    for (auto& gameObject : m_GameObjects)
    {
        gameObject->SetVisible(visible);
    }
}

void Layer::SetAllObjectActive(bool active)
{
    for (auto& gameObject : m_GameObjects)
    {
        gameObject->SetActive(active);
    }
}
#pragma endregion

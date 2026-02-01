#pragma once

#include "Scene.h"
#include "Layer.h"
#include "GameObject.h"

#pragma region Constructor&Destructor
EResult Scene::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
		m_Name = desc->name;
	}
	return EResult::Success;
}

Scene* Scene::Create(void* arg)
{
	Scene* instance = new Scene();
	
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void Scene::Free()
{
	for (auto& layer : m_Layers)
	{
		Safe_Release(layer);
	}
	m_Layers.clear();
}
#pragma endregion

#pragma region Loop
void Scene::FixedUpdate(f32 dt)
{
	for (auto& layer : m_Layers)
	{
		layer->FixedUpdate(dt);
	}
}

void Scene::Update(f32 dt)
{
	for (auto& layer : m_Layers)
	{
		layer->Update(dt);
	}
}

void Scene::LateUpdate(f32 dt)
{
	for (auto& layer : m_Layers)
	{
		layer->LateUpdate(dt);
	}
}
#pragma endregion

#pragma region Layer Management
EResult Scene::AddLayer(class Layer* layer)
{
	if (!layer) return EResult::InvalidArgument;

	if (find(m_Layers.begin(), m_Layers.end(), layer) != m_Layers.end())
		return EResult::AlreadyInitialized;

	m_Layers.push_back(layer);
	layer->SetIndex(static_cast<uint32>(m_Layers.size() - 1));
	return EResult::Success;
}

EResult Scene::InsertLayer(uint32 layerIndex, class Layer* layer)
{
	if (!layer) return EResult::InvalidArgument;

	if (layerIndex > m_Layers.size())
		return AddLayer(layer);

	m_Layers.insert(m_Layers.begin() + layerIndex, layer);
	UpdateLayerIndices(layerIndex);
	return EResult::Success;
}

EResult Scene::CreateLayer(const wstring& layerName, uint32 layerIndex)
{
	tagLayerCreateDesc layerCreateDesc{ layerIndex, layerName };
	Layer* newLayer = Layer::Create(&layerCreateDesc);
	if (!newLayer) return EResult::Fail;

	if (layerIndex != static_cast<uint32>(-1))
	{
		return InsertLayer(layerIndex, newLayer);
	}
	else
	{
		return AddLayer(newLayer);
	}

	return EResult::Success;
}

EResult Scene::RemoveLayer(uint32 layerIndex)
{
	if (layerIndex >= m_Layers.size()) return EResult::InvalidArgument;

	Layer* targetLayer = m_Layers[layerIndex];

	Safe_Release(targetLayer);
	m_Layers.erase(m_Layers.begin() + layerIndex);
	UpdateLayerIndices(layerIndex);
	return EResult::Success;
}

EResult Scene::RemoveLayer(class Layer* layer)
{
	if (!layer) return EResult::InvalidArgument;
	auto it = find(m_Layers.begin(), m_Layers.end(), layer);
	if (it == m_Layers.end()) return EResult::Fail;
	uint32 index = static_cast<uint32>(std::distance(m_Layers.begin(), it));
	return RemoveLayer(index);
}

EResult Scene::RemoveLayer(const wstring& layerName)
{
	Layer* layer = FindLayer(layerName);
	if (!layer) return EResult::Fail;
	return RemoveLayer(layer);
}

Layer* Scene::FindLayer(uint32 layerIndex) const
{
	if (layerIndex >= m_Layers.size()) return nullptr;
	return m_Layers[layerIndex];
}

Layer* Scene::FindLayer(const wstring& layerName) const
{
	for (auto& layer : m_Layers)
	{
		if (layer->GetName() == layerName)
			return layer;
	}
	return nullptr;
}

void Scene::UpdateLayerIndices(uint32 startIndex)
{
	for (uint32 i = startIndex; i < m_Layers.size(); ++i)
	{
		m_Layers[i]->SetIndex(i);
	}
}

EResult Scene::ReorderLayer(uint32 oldIndex, uint32 newIndex)
{
	if (oldIndex == newIndex) return EResult::Success;
	if (oldIndex >= m_Layers.size() || newIndex >= m_Layers.size())	return EResult::InvalidArgument;
	Layer* layer = m_Layers[oldIndex];
	m_Layers.erase(m_Layers.begin() + oldIndex);
	m_Layers.insert(m_Layers.begin() + newIndex, layer);
	uint32 startIndex = std::min(oldIndex, newIndex);
	UpdateLayerIndices(startIndex);
	return EResult::Success;
}

void Scene::SetLayerName(uint32 layerIndex, const wstring& name)
{
	if (layerIndex < m_Layers.size())
		m_Layers[layerIndex]->SetName(name);
}
#pragma endregion

#pragma region Object Management
EResult Scene::AddGameObject(class GameObject* gameObject, uint32 layerIndex)
{
	if (!gameObject) return EResult::InvalidArgument;
	if (layerIndex >= m_Layers.size()) layerIndex = 0;
	return m_Layers[layerIndex]->AddGameObject(gameObject);
}

EResult Scene::RemoveGameObject(class GameObject* gameObject)
{
	if (!gameObject) return EResult::InvalidArgument;
	uint32 layerIndex = gameObject->GetLayerIndex();
	if (layerIndex >= m_Layers.size()) return EResult::InvalidArgument;
	return m_Layers[layerIndex]->RemoveGameObject(gameObject);
}

EResult Scene::MoveGameObjectLayer(class GameObject* gameObject, uint32 targetLayerIndex)
{
	if (!gameObject) return EResult::InvalidArgument;
	if (targetLayerIndex >= m_Layers.size()) return EResult::InvalidArgument;

	uint32 currentLayerIndex = gameObject->GetLayerIndex();
	if (currentLayerIndex == targetLayerIndex)
		return EResult::Success;
	Safe_AddRef(gameObject);

	if (currentLayerIndex < m_Layers.size())
	{
		if (IsFailure(m_Layers[currentLayerIndex]->RemoveGameObject(gameObject)))
		{
			Safe_Release(gameObject);
			return EResult::Fail;
		}
	}
	else
	{
		Safe_Release(gameObject);
		return EResult::Fail;
	}

	if (IsFailure(m_Layers[currentLayerIndex]->AddGameObject(gameObject)))
	{
		Safe_Release(gameObject);
		return EResult::Fail;
	}
	Safe_Release(gameObject);
	return EResult::Success;
}

#pragma endregion

#pragma region Flag Management
void Scene::SetActive(bool active)
{
	if (active)
	{
		AddFlag(m_Flags, ESceneFlags::Active);
	}
	else
	{
		RemoveFlag(m_Flags, ESceneFlags::Active);
	}

	for (auto& Layer : m_Layers)
	{
		Layer->SetActive(active);
	}
}
#pragma endregion

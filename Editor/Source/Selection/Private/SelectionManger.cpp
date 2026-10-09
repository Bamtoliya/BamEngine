#pragma once

#include "SelectionManager.h"
#include "CollisionManager.h"

#include "SceneManager.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "TransformSystem.h"
#include "Mesh.h"
#include "MaterialInterface.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_SINGLETON(SelectionManager)

#pragma region Constructor&Destructor
EResult SelectionManager::Initialize(void* arg)
{
	return EResult::Success;
}

void SelectionManager::Free()
{
	m_SelectedObjects.clear();
}
#pragma endregion

#pragma region Object Selection
vector<class GameObject*>& SelectionManager::GetSelectionContext()
{
	return m_SelectedObjects;
}

GameObject* SelectionManager::GetPrimarySelection()
{
	return m_SelectedObjects.empty() ? nullptr : m_SelectedObjects.back();
}

void SelectionManager::ToggleSelection(GameObject* gameObject)
{
	if (m_LockPrimarySeletion) return;
	auto it = std::find(m_SelectedObjects.begin(), m_SelectedObjects.end(), gameObject);
	if (it != m_SelectedObjects.end())
	{
		m_SelectedObjects.erase(it);
	}
	else
	{
		m_SelectedObjects.push_back(gameObject);
	}
}

void SelectionManager::ClearSelection(bool force)
{
	if (m_LockPrimarySeletion && !force) return;
	m_SelectedEntities.clear();
	m_SelectedObjects.clear();
}

void SelectionManager::SetSelectedObject(GameObject* gameObject)
{
	if (m_LockPrimarySeletion) return;
	ClearSelection();
	if (gameObject)
	{
		m_SelectedObjects.push_back(gameObject);
	}
}

bool SelectionManager::IsSelected(GameObject* gameObject) const
{
	return find(m_SelectedObjects.begin(), m_SelectedObjects.end(), gameObject) != m_SelectedObjects.end();
}

void SelectionManager::AddToSelection(GameObject* gameObject)
{
	if (!gameObject) return;
	if (!IsSelected(gameObject))
	{
		m_SelectedObjects.push_back(gameObject);
	}
}

GameObject* SelectionManager::PickObjectByRay(const Ray& ray)
{
	HitResult hitResult;
	if (CollisionManager::Get().Raycast(ray, hitResult))
	{
		GameObject* hitObject = static_cast<GameObject*>(hitResult.userData);
		if (hitObject)
		{
			return hitObject;
		}
	}
	return nullptr;
}
#pragma endregion

#pragma region Entity Selection
vector<class Entity*>& SelectionManager::GetSelectedEntities()
{
	return m_SelectedEntities;
}

Entity* SelectionManager::GetPrimarySelectedEntity()
{
	return m_SelectedEntities.empty() ? nullptr : m_SelectedEntities.back();;
}

void SelectionManager::ToggleEntitySelection(Entity* entity)
{
	if (m_LockPrimarySeletion) return;
	auto it = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity);
	if (it != m_SelectedEntities.end())
	{
		m_SelectedEntities.erase(it);
	}
	else
	{
		m_SelectedEntities.push_back(entity);
	}
}

void SelectionManager::SetSelectedEntity(Entity* entity)
{
	if (m_LockPrimarySeletion) return;
	ClearSelection();
	if (entity)
	{
		m_SelectedEntities.push_back(entity);
	}
}

bool SelectionManager::IsEntitySelected(Entity* entity) const
{
	return find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity) != m_SelectedEntities.end();
}

void SelectionManager::AddToEntitySelection(Entity* entity)
{
	if (!entity) return;
	if (!IsEntitySelected(entity))
	{
		m_SelectedEntities.push_back(entity);
	}
}

Entity* SelectionManager::PickEntityByRay(const struct Ray& ray, float maxDistance)
{
	TODO("Implement entity picking by raycasting. This may involve checking for collisions with entities in the scene and returning the closest hit entity.");
	return nullptr;
}

#pragma endregion


#pragma region Asset Selection
void SelectionManager::SetSelectedAsset(const filesystem::path& assetPath)
{
	m_LastSelectedAssetPath.clear();
	m_LastSelectedAssetPath = assetPath;
}

filesystem::path SelectionManager::GetSelectedAssetPath() const
{
	return m_LastSelectedAssetPath;
}
void SelectionManager::SetSelectedResource(const filesystem::path& assetPath)
{
	m_LastSelectedResourcePath.clear();
	m_LastSelectedResourcePath = assetPath;
}
filesystem::path SelectionManager::GetSelectedAssetResource() const
{
	return m_LastSelectedResourcePath;
}
#pragma endregion
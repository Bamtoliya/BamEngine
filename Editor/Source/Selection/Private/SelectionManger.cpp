#pragma once

#include "SelectionManager.h"
#include "CollisionManager.h"

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

void SelectionManager::ClearSelection()
{
	m_SelectedObjects.clear();
}

void SelectionManager::SetSelectedObject(GameObject* gameObject)
{
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
		GameObject* hitObject = static_cast<GameObject*>(hitResult.UserData);
		if (hitObject)
		{
			return hitObject;
		}
	}
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


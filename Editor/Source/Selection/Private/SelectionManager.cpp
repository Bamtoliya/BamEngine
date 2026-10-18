#pragma once

#include "SelectionManager.h"

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
	ClearSelection();
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

void SelectionManager::ToggleSelection(Entity* entity)
{
	if (m_LockPrimarySeletion || !entity) return;
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

void SelectionManager::AddToSelection(Entity* entity)
{
	if (!entity) return;
	if (!IsEntitySelected(entity))
	{
		m_SelectedEntities.push_back(entity);
	}
}

void SelectionManager::ClearSelection(bool force)
{
	if (!force && m_LockPrimarySeletion) return;
	m_SelectedEntities.clear();
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
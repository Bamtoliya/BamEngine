#pragma once

#include "SelectionManager.h"

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
vector<Entity*> SelectionManager::GetSelectedEntities()
{
	vector<Entity*> selected;
	selected.reserve(m_SelectedEntities.size());

	for (const auto& ref : m_SelectedEntities)
		if (auto* entity = Entity::Resolve(ref)) selected.push_back(entity);

	return selected;
}

Entity* SelectionManager::GetPrimarySelectedEntity()
{
	for (auto it = m_SelectedEntities.rbegin(); it != m_SelectedEntities.rend(); ++it)
		if (auto* entity = Entity::Resolve(*it)) return entity;

	return nullptr;
}

void SelectionManager::ToggleSelection(Entity* entity)
{
	if (m_LockPrimarySeletion || !entity) return;

	std::erase_if(m_SelectedEntities, [](const auto& ref) { return !Entity::Resolve(ref); });
	const auto it = std::find_if(m_SelectedEntities.begin(), m_SelectedEntities.end(),
		[entity](const auto& ref) { return Entity::Resolve(ref) == entity; });

	if (it != m_SelectedEntities.end())
		m_SelectedEntities.erase(it);
	else
		m_SelectedEntities.push_back(entity->GetWeakRef());
}

void SelectionManager::SetSelectedEntity(Entity* entity)
{
	if (m_LockPrimarySeletion) return;

	ClearSelection();
	if (entity)
		m_SelectedEntities.push_back(entity->GetWeakRef());
}

bool SelectionManager::IsEntitySelected(Entity* entity) const
{
	if (!entity) return false;

	return std::any_of(m_SelectedEntities.begin(), m_SelectedEntities.end(),
		[entity](const auto& ref) { return Entity::Resolve(ref) == entity; });
}

void SelectionManager::AddToSelection(Entity* entity)
{
	if (!entity) return;

	std::erase_if(m_SelectedEntities, [](const auto& ref) { return !Entity::Resolve(ref); });
	if (!IsEntitySelected(entity))
		m_SelectedEntities.push_back(entity->GetWeakRef());
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
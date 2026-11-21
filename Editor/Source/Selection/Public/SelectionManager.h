#pragma once

#include "Editor_Includes.h"
#include "Entity.h"

BEGIN(Editor)
class SelectionManager : public Base
{
	DECLARE_SINGLETON(SelectionManager)

#pragma region Constructor & Destructor
public:
	SelectionManager() {}
	virtual ~SelectionManager() = default;
	EResult Initialize(void* arg = nullptr);
private:
	virtual void Free() override;
#pragma endregion

#pragma region Entity Selection
public:
	vector<Entity*> GetSelectedEntities();
	Entity* GetPrimarySelectedEntity();
	void ToggleSelection(Entity* entity);
	void ToggleEntitySelection(Entity* entity) { ToggleSelection(entity); }
	void SetSelectedEntity(Entity* entity);
	bool IsEntitySelected(Entity* entity) const;
	void AddToSelection(Entity* entity);
	void AddToEntitySelection(Entity* entity) { AddToSelection(entity); }
	void ClearSelection(bool force = false);
public:
	bool IsPrimarySelectionLocked() { return m_LockPrimarySeletion; }
	void SetPrimarySelectionLock(bool lock = true) { m_LockPrimarySeletion = lock; }
	void TogglePrimarySelectionLock() { m_LockPrimarySeletion = !m_LockPrimarySeletion; }
#pragma endregion


#pragma region Asset Selection
public:
	void SetSelectedAsset(const filesystem::path& assetPath);
	filesystem::path GetSelectedAssetPath() const;
#pragma endregion

#pragma region Resource Selection
public:
	void SetSelectedResource(const filesystem::path& assetPath);
	filesystem::path GetSelectedAssetResource() const;
#pragma endregion

private:
	bool m_LockPrimarySeletion = { false };
	vector<Entity::WeakRef> m_SelectedEntities;
	filesystem::path m_LastSelectedAssetPath;
	filesystem::path m_LastSelectedResourcePath;
};
END

#pragma once

#include "Base.h"
#include "Editor_Includes.h"

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


#pragma region Object Selection
public:
	vector<class GameObject*>& GetSelectionContext();
	class GameObject* GetPrimarySelection();
	void ToggleSelection(class GameObject* gameObject);
	void ClearSelection(); 
	void SetSelectedObject(class GameObject* gameObject);
	bool IsSelected(class GameObject* gameObject) const;
	void AddToSelection(class GameObject* gameObject);
	class GameObject* PickObjectByRay(const struct Ray& ray);
#pragma endregion

#pragma region Asset Selection
public:
	void SetSelectedAsset(const filesystem::path& assetPath);
	filesystem::path GetSelectedAssetPath() const;
#pragma endregion

private:
	vector<class GameObject*> m_SelectedObjects;
	filesystem::path m_LastSelectedAssetPath;
};
END
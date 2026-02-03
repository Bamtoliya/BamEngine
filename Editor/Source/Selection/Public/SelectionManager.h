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
	EResult Initialize(void* arg = nullptr) { return EResult::Success; }
private:
	virtual void Free() override;
#pragma endregion


#pragma region Getter
public:
	vector<class GameObject*>& GetSelectionContext() { return m_SelectedObjects; }
	class GameObject* GetPrimarySelection() { return m_SelectedObjects.empty() ? nullptr : m_SelectedObjects.back(); }
	void ToggleSelection(class GameObject* gameObject)
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
	void ClearSelection() {
		m_SelectedObjects.clear();
	}
	bool IsSelected(class GameObject* gameObject) const
	{
		return find(m_SelectedObjects.begin(), m_SelectedObjects.end(), gameObject) != m_SelectedObjects.end();
	}
#pragma endregion

private:
	vector<class GameObject*> m_SelectedObjects;
};
END
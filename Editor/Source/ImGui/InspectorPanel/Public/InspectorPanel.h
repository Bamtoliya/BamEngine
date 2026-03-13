#pragma once

#include "ImGuiInterface.h"

BEGIN(Editor)
class InspectorPanel final : public ImGuiInterface
{
public:
	InspectorPanel() { m_Name = L"Inspector Panel"; }
	virtual ~InspectorPanel() = default;
public:
	virtual void Draw() override;
	virtual void Free() override { __super::Free(); }
private:
	bool DrawProperties(void* instance, const TypeInfo& typeInfo);
public:
	EResult SetSelectedGameObject(class GameObject* gameObject) { m_SelectedGameObject = gameObject; return EResult::Success; }

#pragma region Add Component
private:
	void DrawAddComponentButton();
	void DrawAddComponentPopup();
#pragma endregion

#pragma region Component Menu
private:
	void DrawComponentMenuButton();
	void DrawComponentMenuPopup();
#pragma endregion

private:
	class GameObject* m_SelectedGameObject = nullptr;
};
END
#pragma once

#include "ImGuiInterface.h"
#include "InspectorInterface.h"

BEGIN(Editor)
class InspectorPanel final : public ImGuiInterface
{
public:
	InspectorPanel();
	virtual ~InspectorPanel() = default;
public:
	virtual void Free() override;
	virtual void Draw() override;
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

#pragma region Components
private:
	void DrawRenderComponentMaterialEditor(class RenderComponent* renderComponent);
#pragma endregion


private:
	class GameObject* m_SelectedGameObject = nullptr;
	vector<InspectorInterface*> m_AssetInspectors;
};
END
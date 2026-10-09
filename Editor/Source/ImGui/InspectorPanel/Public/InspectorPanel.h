#pragma once

#include "ImGuiInterface.h"
#include "InspectorInterface.h"
#include "ReflectionPropertyDrawer.h"
#include "ReflectionEditState.h"

BEGIN(Engine)
class GameObject;
class Entity;
END

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
	bool DrawProperties(entt::meta_any& instance, const entt::meta_type& type);
	void DrawEntityProperties(Entity& entity);
public:
	EResult SetSelectedGameObject(GameObject* gameObject) { m_SelectedGameObject = gameObject; return EResult::Success; }
	EResult SetSelectedEntity(Entity* entity);

#pragma region Add Component
private:
	void DrawAddComponentButton();
	void DrawAddComponentPopup();
	void DrawAddComponentButton(Entity& entity);
	void DrawAddComponentPopup(Entity& entity);
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
	GameObject* m_SelectedGameObject = nullptr;
	vector<InspectorInterface*> m_AssetInspectors;
};
END
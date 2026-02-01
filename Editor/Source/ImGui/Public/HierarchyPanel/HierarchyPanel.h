#pragma once

#include "Base.h"
#include "Editor_Includes.h"

BEGIN(Editor)

class HierarchyPanel
{
public:
	void Draw(class GameObject*& selectedObject);

private:
	void DrawLayerItem(class Layer* layer, class GameObject*& selectedObject, class Scene* scene);
	void DrawGameObjectNode(class GameObject* gameObject, class GameObject*& selectedObject);

	void DrawAddGameObjectButton(Scene* currentScene, GameObject*& selectedObject);

	void SelectObject(class GameOjbect* gameObject, bool multiSelect);
	bool IsSelected(class GameObject* gameObject) const;
	void ClearSelection();


private:
	void CreateEmptyObject(class Scene* scene, class GameObject*& selectedObject);
	void CreatePrimitive(class Scene* scene, class GameObject*& selectedObject, const wstring& name, const wstring& meshName);

private:
	vector<class GameObject*> m_SelectedObjects;
	class GameObject* m_LastSelectedObject = { nullptr };

	bool m_isRenamingLayer = false;
	uint32 m_RenamingLayerIndex = -1;
	char m_RenameBuffer[256] = "";
};
END
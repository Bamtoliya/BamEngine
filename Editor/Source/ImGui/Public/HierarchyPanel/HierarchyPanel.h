#pragma once

#include "Base.h"
#include "Editor_Includes.h"

BEGIN(Editor)

class HierarchyPanel
{
public:
	void Draw();

private:
	void DrawLayerItem(class Scene* scene, class Layer* layer);
	void DrawGameObjectNode(class GameObject* gameObject);

	void DrawAddGameObjectButton(class Scene* currentScene);
private:
	void DrawSceneTitle(class Scene* scene);
private:
	void DrawLayerContextMenu(class Scene* scene);
private:
	void CreateEmptyObject(class Scene* scene);
	void CreatePrimitive(class Scene* scene, const wstring& name, const wstring& meshName);


#pragma region Variables
private:
	bool m_isRenamingLayer = false;
	uint32 m_RenamingLayerIndex = -1;
	char m_RenameBuffer[256] = "";
#pragma endregion
};
END
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
	template<typename T>
	bool DrawRenameBox(T* target, ImGuiTreeNodeFlags flags, bool isSelected);
	
private:
	void DrawSceneContextMenu();
	void DrawLayerContextMenu(class Scene* scene);
	void DrawGameObjectContextMenu();
private:
	void CreateEmptyObject(class Scene* scene);
	void CreatePrimitive(class Scene* scene, const wstring& name, const wstring& meshName);





#pragma region Variables
private:
	void* m_RenamingId = { nullptr };
	char m_RenameBuffer[256] = "";
#pragma endregion
};
END
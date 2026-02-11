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
	bool DrawRenameBox(T* target, ImGuiTreeNodeFlags flags, bool isSelected,
						function<void()> onDragDrop = nullptr,
						function<void()> onContextMenu = nullptr);
private:
	void DrawSceneContextMenu();
	void DrawLayerContextMenu(class Layer* layer);
	void DrawGameObjectContextMenu(class GameObject* gameObject);
private:
	void CreateEmptyObject(class Scene* scene);
	void CreatePrimitive(class Scene* scene, const wstring& name, const wstring& meshName);
	void CreateSpriteObject(class Scene* scene);
	void CreateCamera(class Scene* scene);





#pragma region Variables
private:
	void* m_RenamingId = { nullptr };
	char m_RenameBuffer[256] = "";
private:
	bool m_IsBoxSelecting = false;      // 현재 박스 선택 중인가?
	ImVec2 m_BoxStartPos = { 0, 0 };    // 드래그 시작 위치 (스크린 좌표)
	ImVec2 m_BoxEndPos = { 0, 0 };      // 드래그 현재 위치 (스크린 좌표)

	// 박스 선택 중일 때 임시로 선택될 객체들을 담을 리스트 (매 프레임 갱신)
	std::vector<class GameObject*> m_BoxSelectionCandidates;
#pragma endregion
};
END
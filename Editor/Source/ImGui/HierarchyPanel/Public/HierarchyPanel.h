#pragma once

#include "Base.h"
#include "ImGuiInterface.h"
#include "Editor_Includes.h"
#include "EntityHierarchyView.h"

BEGIN(Editor)

class HierarchyPanel : public ImGuiInterface
{
public:
	HierarchyPanel() { m_Name = L"Hierarchy Panel"; m_DisplayNameKey = "UI_HIERARCHY"; }
	virtual ~HierarchyPanel() = default;
public:
	virtual void Draw() override;
	virtual void Free() override { m_EntityTree.ResetDrag(); __super::Free(); }
private:
	void DrawSceneTitle(class Scene* scene);
private:
	template<typename T>
	bool DrawRenameBox(T* target, ImGuiTreeNodeFlags flags, bool isSelected,
						function<void()> onDragDrop = nullptr,
						function<void()> onContextMenu = nullptr);
private:
	void DrawSceneContextMenu(class Scene* scene);

private:
	void DrawEntityHierarchy(class Scene* scene);
	void DrawEntityName(const wstring& name, bool isSelected);
	void DrawEntityRenameBox(class Entity* entity, bool isSelected);
private:
	void DrawEntityContextMenu(class Entity* entity);

#pragma region Variables
private:
	EntityHierarchyView m_EntityTree;
	Engine::Scene* m_LastHierarchyScene = nullptr;
	entt::entity m_LastHierarchyPrimary = entt::null;
	entt::entity m_RevealEntity = entt::null;
	std::string m_HierarchyMessage;
	void* m_RenamingId = { nullptr };
	char m_RenameBuffer[256] = "";
	char m_SearchFilter[128] = "";
private:
	bool m_IsBoxSelecting = false;      // 현재 박스 선택 중인가?
	ImVec2 m_BoxStartPos = { 0, 0 };    // 드래그 시작 위치 (스크린 좌표)
	ImVec2 m_BoxEndPos = { 0, 0 };      // 드래그 현재 위치 (스크린 좌표)
#pragma endregion
};
END
#pragma once

#include "ContentBrowserTreeItem.h"
#include "ContentBrowserPanel.h"
#include "ImGuiManager.h"
#include "ShortcutManager.h"
#include "Editor_Macros.h"

bool ContentBrowserTreeItem::DrawNode(ContentBrowserPanel& browser, const std::filesystem::path& path,
	const std::filesystem::path& rootPath, bool hasChildDirectory, bool isSelected, bool needToExpand,
	bool isReadOnly, bool isEngineRoot, bool isEditorRoot)
{
	ImGuiTreeNodeFlags baseFlags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0);
	baseFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	// 트리 자동 확장 로직
	if (needToExpand)
	{
		try {
			auto rel = browser.GetCurrentDirectory().lexically_relative(path);
			bool isAncestor = !rel.empty() && rel.string().find("..") == std::string::npos;
			if (isAncestor)
			{
				ImGui::SetNextItemOpen(true);
			}
		}
		catch (...) {}
	}

	if (!hasChildDirectory)
	{
		baseFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	std::string filename = Engine::Unicode::WideToUtf8(path.filename().wstring());
	std::string icon = (isSelected && hasChildDirectory) ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER;
	if (path == rootPath)
	{
		if (isEngineRoot)
			icon = ICON_FA_GEARS;
		else if (isEditorRoot)
			icon = ICON_FA_TOOLBOX;
		else
			icon = ICON_FA_DATABASE;
	}

	bool isRenaming = (browser.m_RenamingPath == path && browser.m_RenameInTree);
	std::string label = isRenaming ? "" : (icon + " " + filename + (isReadOnly && path != rootPath ? (" " ICON_FA_LOCK) : ""));
	if (path == rootPath && !isRenaming)
	{
		if (isEngineRoot)
			label = icon + " " + LOCAL("ContentBrowser.EngineResources");
		else if (isEditorRoot)
			label = icon + " " + LOCAL("ContentBrowser.EditorResources");
		else
			label = icon + " " + LOCAL("ContentBrowser.Assets");
	}

	// 트리 노드 렌더링
	bool opened = ImGui::TreeNodeEx((void*)std::hash<std::wstring>{}(path.native()), baseFlags, "%s", label.c_str());

	if (!isRenaming) browser.DrawFolderDropTarget(path);

	// 인라인 이름 변경 UI
	if (isRenaming)
	{
		ImGui::SameLine();
		DrawRename(browser, path);
	}

	// 컨텍스트 메뉴 및 단축키(F2) 처리
	if (!isRenaming)
	{
		ContextMenu(browser, path, rootPath);
		if (ImGui::IsItemClicked())
			browser.NavigateTo(path);
	}

	// 자식 노드가 있고, 트리가 열렸을 때만 true 반환
	return opened && hasChildDirectory;
}

void ContentBrowserTreeItem::ContextMenu(ContentBrowserPanel& browser, const std::filesystem::path& path,
	const std::filesystem::path& rootPath)
{
	if (ImGui::IsItemHovered())
		browser.SetRenameTarget(path, true);
	if (ImGui::BeginPopupContextItem())
	{
		browser.SetRenameTarget(path, true);

		const bool isReadOnly = Engine::PlatformPath::IsReadOnlyTarget(path);

		browser.DrawFileClipboardMenu(path == rootPath ? std::vector<std::filesystem::path>{} :
			std::vector<std::filesystem::path>{ path }, path);
		ImGui::Separator();

		ImGui::BeginDisabled(isReadOnly || !browser.CanUseFileCommands());
		if (ImGui::MenuItem((ICON_FA_PLUS " " + LOCAL_LABEL("EditorCommon.NewFolder")).c_str()))
		{
			browser.CreateFolder(path);
		}

		ACTION_MENU_ITEM((ICON_FA_PEN " " + LOCAL_LABEL("EditorCommon.Rename")).c_str(), "Edit.Rename");
		ImGui::EndDisabled();

		ImGui::Separator();

		if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.ShowInExplorer")).c_str()))
		{
			ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
		}

		browser.DrawPathCopyMenu({ path });

		ImGui::EndPopup();
	}
}

void ContentBrowserTreeItem::DrawRename(ContentBrowserPanel& browser, const std::filesystem::path& path)
{
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	browser.DrawRenameEditor("##TreeRenameInput", path);
}
#pragma once

#include "ContentBrowserTreeItem.h"
#include "ContentBrowserPanel.h"
#include "ImGuiManager.h"
#include "ShortcutManager.h"
#include "Editor_Macros.h"
#include "ContentBrowserRename.h"

bool ContentBrowserTreeItem::DrawNode(
	const std::filesystem::path& path,
	const std::filesystem::path& rootPath,
	bool hasChildDirectory,
	bool isSelected,
	bool needToExpand,
	std::filesystem::path& outRenamingPath,
	char* renameBuffer,
	size_t renameBufferSize,
	std::filesystem::path& currentDirectory)
{
	ImGuiTreeNodeFlags baseFlags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0);
	baseFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	// 트리 자동 확장 로직
	if (needToExpand)
	{
		try {
			auto rel = currentDirectory.lexically_relative(path);
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

	const bool isReadOnly = Engine::PlatformPath::IsReadOnlyTarget(path);
	auto* browser = dynamic_cast<ContentBrowserPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Content Browser"));
	const bool isEngineRoot = (browser && path == browser->GetEngineRootPath());
	const bool isEditorRoot = (browser && path == browser->GetEditorRootPath());

	std::string filename = path.filename().string();
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

	bool isRenaming = (outRenamingPath == path);
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
	bool opened = ImGui::TreeNodeEx((void*)std::hash<std::string>{}(path.string()), baseFlags, "%s", label.c_str());

	// 인라인 이름 변경 UI
	if (isRenaming)
	{
		ImGui::SameLine();
		DrawRename(path, outRenamingPath, renameBuffer, renameBufferSize, currentDirectory);
	}

	// 컨텍스트 메뉴 및 단축키(F2) 처리
	ContextMenu(path, rootPath, outRenamingPath, renameBuffer, renameBufferSize, currentDirectory);

	// 클릭 시 디렉토리 이동
	if (ImGui::IsItemClicked())
	{
		currentDirectory = path;
	}

	// 자식 노드가 있고, 트리가 열렸을 때만 true 반환
	return opened && hasChildDirectory;
}

void ContentBrowserTreeItem::ContextMenu(const std::filesystem::path& path, const std::filesystem::path& rootPath, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory)
{
	auto* browser = dynamic_cast<ContentBrowserPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Content Browser"));
	if (browser && ImGui::IsItemHovered())
		browser->SetRenameTarget(path);
	if (ImGui::BeginPopupContextItem())
	{
		if (browser)
			browser->SetRenameTarget(path);

		currentDirectory = path;
		const bool isReadOnly = Engine::PlatformPath::IsReadOnlyTarget(path);

		ImGui::BeginDisabled(isReadOnly);
		if (ImGui::MenuItem((ICON_FA_PLUS " " + LOCAL_LABEL("EditorCommon.NewFolder")).c_str()))
		{
			std::filesystem::path newFolderPath = path / "NewFolder";
			int suffix = 1;
			while (std::filesystem::exists(newFolderPath))
			{
				newFolderPath = path / ("NewFolder_" + std::to_string(suffix++));
			}
			std::filesystem::create_directory(newFolderPath);
		}

		ACTION_MENU_ITEM((ICON_FA_PEN " " + LOCAL_LABEL("EditorCommon.Rename")).c_str(), "Edit.Rename");
		ImGui::EndDisabled();

		ImGui::Separator();

		if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.ShowInExplorer")).c_str()))
		{
			ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
		}

		if (ImGui::MenuItem((ICON_FA_COPY " " + LOCAL_LABEL("EditorCommon.CopyPath")).c_str()))
		{
			ImGui::SetClipboardText(path.string().c_str());
		}

		ImGui::Separator();

		if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str(), nullptr, false,
			path != rootPath && !isReadOnly))
		{
			std::error_code ec;
			if (currentDirectory == path)
				currentDirectory = path.parent_path();
			std::filesystem::remove_all(path, ec);
			if (browser)
				browser->RequestRefresh();
		}

		ImGui::EndPopup();
	}
}

void ContentBrowserTreeItem::DrawRename(const std::filesystem::path& path, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory)
{
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	Editor::ContentBrowser::DrawRenameInput("##TreeRenameInput", path, outRenamingPath, renameBuffer, renameBufferSize, currentDirectory);
}
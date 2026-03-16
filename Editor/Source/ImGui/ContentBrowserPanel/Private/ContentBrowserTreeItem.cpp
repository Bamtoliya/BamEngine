#pragma once

#include "ContentBrowserTreeItem.h"

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
			auto rel = std::filesystem::relative(currentDirectory, path);
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

	std::string filename = path.filename().string();
	std::string icon = (isSelected && hasChildDirectory) ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER;
	if (path == rootPath) icon = ICON_FA_DATABASE;

	bool isRenaming = (outRenamingPath == path);
	std::string label = isRenaming ? "" : (icon + " " + filename);
	if (path == rootPath && !isRenaming) label = ICON_FA_DATABASE " Assets";

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
	std::string filename = path.filename().string();
	if (ImGui::BeginPopupContextItem())
	{
		currentDirectory = path;

		if (ImGui::MenuItem(ICON_FA_PLUS " New Folder"))
		{
			std::filesystem::path newFolderPath = path / "NewFolder";
			int suffix = 1;
			while (std::filesystem::exists(newFolderPath))
			{
				newFolderPath = path / ("NewFolder_" + std::to_string(suffix++));
			}
			std::filesystem::create_directory(newFolderPath);
		}

		if (ImGui::MenuItem(ICON_FA_PEN " Rename", "F2"))
		{
			outRenamingPath = path;
			strncpy(renameBuffer, filename.c_str(), renameBufferSize);
		}

		ImGui::Separator();

		if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Show in Explorer"))
		{
			ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
		}

		if (ImGui::MenuItem(ICON_FA_COPY " Copy Path"))
		{
			ImGui::SetClipboardText(path.string().c_str());
		}

		ImGui::Separator();

		if (ImGui::MenuItem(ICON_FA_TRASH " Delete", nullptr, false, path != rootPath))
		{
			// TODO: 삭제 로직
			TODO("TreeItem ContextMenu 삭제 로직 만들어야함")
		}

		ImGui::EndPopup();
	}

	// 호버 상태에서 F2 키 누르면 리네임 모드 진입
	if (ImGui::IsItemHovered() && KEY_DOWN(EKeyCode::F2))
	{
		outRenamingPath = path;
		strncpy(renameBuffer, filename.c_str(), renameBufferSize);
	}
}

void ContentBrowserTreeItem::DrawRename(const std::filesystem::path& path, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory)
{
	ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
	if (ImGui::InputText("##TreeRenameInput", renameBuffer, renameBufferSize, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
	{
		std::filesystem::path newPath = path.parent_path() / renameBuffer;
		if (renameBuffer[0] != '\0' && !std::filesystem::exists(newPath))
		{
			try {
				std::filesystem::rename(path, newPath);
				if (currentDirectory == path) currentDirectory = newPath;
			}
			catch (...) {}
		}
		outRenamingPath.clear();
	}

	if (ImGui::IsItemVisible() && !ImGui::IsAnyItemActive())
		ImGui::SetKeyboardFocusHere(-1);

	if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit())
	{
		outRenamingPath.clear();
	}
	else if (KEY_DOWN(EKeyCode::Escape))
	{
		outRenamingPath.clear();
	}
}
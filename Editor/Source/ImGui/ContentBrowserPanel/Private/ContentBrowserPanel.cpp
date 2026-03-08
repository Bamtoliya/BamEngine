#pragma once
#include "ContentBrowserPanel.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h> 

EResult ContentBrowserPanel::Initialize(void* arg)
{
	m_Name = L"Content Browser";
	filesystem::path resourcePath = filesystem::current_path();
	resourcePath.append("Resources");
	m_RootPath = resourcePath;
	m_CurrentDirectory = m_RootPath;
	m_LastDirectory = m_CurrentDirectory;
	m_NeedToExpandTree = true;
	return EResult::Success;
}

void ContentBrowserPanel::Draw()
{
	if (!m_Open) return;
	ImGui::Begin("Content Browser", &m_Open);

	if (m_CurrentDirectory != m_LastDirectory)
	{
		m_NeedToExpandTree = true;
		m_LastDirectory = m_CurrentDirectory;
	}

	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;

	if (ImGui::BeginTable("ContentBrowserTableLayout", 2, tableFlags))
	{
		ImGui::TableSetupColumn("Folders", ImGuiTableColumnFlags_WidthFixed, 250.0f);
		ImGui::TableSetupColumn("Files", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();

		ImGui::TableNextColumn();
		ImGui::BeginChild("TreeArea", ImVec2(0, 0), false);
		DrawDirectoryTree(m_RootPath);
		ImGui::EndChild();

		ImGui::TableNextColumn();
		DrawDirectoryGrid();

		ImGui::EndTable();
	}
	if (ImGui::IsWindowAppearing()) m_NeedToExpandTree = true;
	ImGui::End();
	m_NeedToExpandTree = false;
}

void ContentBrowserPanel::Free()
{
	for (auto& [path, texture] : m_ThumbnailTextures)
	{
		if (texture) texture->Release();
	}
	m_ThumbnailTextures.clear();
	m_ThumbnailCache.clear();
}

#pragma region TreeView
void ContentBrowserPanel::DrawDirectoryTree(const filesystem::path& path)
{
	ImGuiTreeNodeFlags baseFlags = ((m_CurrentDirectory == path) ? ImGuiTreeNodeFlags_Selected : 0);
	baseFlags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;


	bool hasChildDirectory = false;
	for (auto& directoryEntry : filesystem::directory_iterator(path))
	{
		if (directoryEntry.is_directory())
		{
			hasChildDirectory = true;
			break;
		}
	}

	if (m_NeedToExpandTree)
	{
		try {
			// relative 결과에 ".."이 없으면 하위 경로임
			auto rel = filesystem::relative(m_CurrentDirectory, path);
			bool isAncestor = !rel.empty() && rel.string().find("..") == string::npos;

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

	string filename = path.filename().string();
	string icon = (m_CurrentDirectory == path && hasChildDirectory) ? ICON_FA_FOLDER_OPEN : ICON_FA_FOLDER;
	if (path == m_RootPath) icon = ICON_FA_DATABASE;

	// 이름 변경 중이면 트리 노드의 텍스트 부분을 비워두거나 커스텀 처리
	bool isRenaming = (m_RenamingPath == path);
	string label = isRenaming ? "" : (icon + " " + filename);
	if (path == m_RootPath && !isRenaming) label = ICON_FA_DATABASE " Assets";

	bool opened = ImGui::TreeNodeEx((void*)std::hash<string>{}(path.string()), baseFlags, "%s", label.c_str());

	// 트리 뷰 인라인 이름 변경 UI 배치
	if (isRenaming)
	{
		ImGui::SameLine();
		DrawRename(path);
	}

	TreeViewContextMenu(path);

	if (ImGui::IsItemClicked())
	{
		m_CurrentDirectory = path;
	}

	if (opened && hasChildDirectory)
	{
		for (auto& entry : std::filesystem::directory_iterator(path))
		{
			if (entry.is_directory())
				DrawDirectoryTree(entry.path());
		}
		ImGui::TreePop();
	}
}

void ContentBrowserPanel::TreeViewContextMenu(const filesystem::path& path)
{
	string filename = path.filename().string();
	if (ImGui::BeginPopupContextItem())
	{
		m_CurrentDirectory = path; // 메뉴를 열 때 해당 디렉토리를 선택 상태로 만듦

		if (ImGui::MenuItem(ICON_FA_PLUS " New Folder"))
		{
			filesystem::path newFolderPath = path / "NewFolder";
			int suffix = 1;
			while (filesystem::exists(newFolderPath))
			{
				newFolderPath = path / ("NewFolder_" + std::to_string(suffix++));
			}
			filesystem::create_directory(newFolderPath);
		}

		if (ImGui::MenuItem(ICON_FA_PEN " Rename", "F2"))
		{
			m_RenamingPath = path;
			strncpy(m_RenameBuffer, filename.c_str(), sizeof(m_RenameBuffer));
		}

		ImGui::Separator();

		if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Show in Explorer"))
		{
			// Windows 전용 명령 (ShellExecute)
			ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
		}

		if (ImGui::MenuItem(ICON_FA_COPY " Copy Path"))
		{
			ImGui::SetClipboardText(path.string().c_str());
		}

		ImGui::Separator();

		if (ImGui::MenuItem(ICON_FA_TRASH " Delete", nullptr, false, path != m_RootPath))
		{
			// 루트가 아닐 때만 삭제 가능 (실제 삭제 시 모달 팝업 권장)
			// filesystem::remove_all(path);
		}

		ImGui::EndPopup();
	}

	if (ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_F2))
	{
		m_RenamingPath = path;
		strncpy(m_RenameBuffer, filename.c_str(), sizeof(m_RenameBuffer));
	}
}
#pragma endregion


#pragma region GridView
void ContentBrowserPanel::DrawDirectoryGrid()
{
	BackButton();
	ImGui::SameLine();
	AddressBar();
	ImGui::Separator();

	f32 cellSize = m_ThumbnailSize + m_Padding;
	f32 panelWidth = ImGui::GetContentRegionAvail().x;
	int32 columnCount = static_cast<int>(panelWidth / cellSize);
	if (columnCount < 1) columnCount = 1;

	if (ImGui::BeginChild("GridArea", ImVec2(0, 0), false))
	{
		GridViewContextMenu(m_CurrentDirectory);
		if (ImGui::BeginTable("ContentBrowserTable", columnCount))
		{
			for (auto& directoryEntry : filesystem::directory_iterator(m_CurrentDirectory))
			{
				const auto& path = directoryEntry.path();
				auto relativePath = filesystem::relative(path, m_RootPath);
				string filenameString = relativePath.filename().string();

				ImGui::TableNextColumn();

				ImGui::PushID(filenameString.c_str());

				DrawThumbnail(directoryEntry);
				GridItemContextMenu(path);
				DragAndDropTarget(relativePath);


				// 인터랙션: 폴더 진입
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
				{
					if (directoryEntry.is_directory())
					{
						m_CurrentDirectory /= path.filename();
					}
				}

				if (m_RenamingPath == path)
				{
					DrawRename(path);
				}
				else
				{
					ImGui::TextWrapped("%s", filenameString.c_str());
				}

				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		ImGui::EndChild();
	}
}

void ContentBrowserPanel::GridViewContextMenu(const filesystem::path& path)
{
	if (ImGui::BeginPopupContextWindow("GridContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		if (ImGui::MenuItem(ICON_FA_PLUS " New Folder"))
		{
			filesystem::path newFolderPath = m_CurrentDirectory / "NewFolder";
			int suffix = 1;
			while (filesystem::exists(newFolderPath))
			{
				newFolderPath = m_CurrentDirectory / ("NewFolder_" + std::to_string(suffix++));
			}
			filesystem::create_directory(newFolderPath);
		}
		ImGui::EndPopup();
	}
}

void ContentBrowserPanel::AddressBar()
{
	static char pathBuffer[1024] = "";
	string relateivePathStr = filesystem::relative(m_CurrentDirectory, m_RootPath).string();
	if (relateivePathStr == ".") relateivePathStr = "";

	if (!ImGui::IsItemActive())
	{
		strncpy(pathBuffer, relateivePathStr.c_str(), sizeof(pathBuffer));
	}

	ImGui::SetNextItemWidth(-1);

	if (ImGui::InputText("##AddressBar", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_EnterReturnsTrue))
	{
		filesystem::path newPath = m_RootPath / pathBuffer;
		if (filesystem::exists(newPath) && filesystem::is_directory(newPath))
		{
			m_CurrentDirectory = newPath;
		}
		else
		{
			strncpy(pathBuffer, relateivePathStr.c_str(), sizeof(pathBuffer));
		}
	}
}

void ContentBrowserPanel::BackButton()
{
	bool isRoot = (m_CurrentDirectory == m_RootPath);

	if (isRoot) ImGui::BeginDisabled();

	if (ImGui::Button(ICON_FA_ARROW_LEFT_LONG))
	{
		m_CurrentDirectory = m_CurrentDirectory.parent_path();
	}

	if (isRoot) ImGui::EndDisabled();
}

void ContentBrowserPanel::DrawThumbnail(const filesystem::directory_entry directoryEntry)
{
	const char* icon = ICON_FA_FILE;
	ImVec4 typeColor = ImVec4(1.0f, 1.0f, 1.0f, 0.1f); // 기본 투명한 회색
	void* thumbnailTexID = nullptr;

	if (directoryEntry.is_directory())
	{
		icon = ICON_FA_FOLDER;
		typeColor = ImVec4(0.9f, 0.7f, 0.2f, 0.3f); // 폴더: 노란색
	}
	else
	{
		string ext = directoryEntry.path().extension().string();
		for (auto& c : ext) c = tolower(c); // 소문자로 변환

		if (ext == ".png" || ext == ".jpg" || ext == ".tga")
		{
			icon = ICON_FA_IMAGE;
			typeColor = ImVec4(0.2f, 0.8f, 0.2f, 0.4f); // 이미지: 초록색
			thumbnailTexID = LoadThumbnail(directoryEntry.path());
		}
		else if (ext == ".fbx" || ext == ".obj")
		{
			icon = ICON_FA_CUBE;
			typeColor = ImVec4(0.2f, 0.4f, 0.9f, 0.4f); // 모델: 파란색
		}
		else if (ext == ".mat")
		{
			icon = ICON_FA_CIRCLE;
			typeColor = ImVec4(0.9f, 0.3f, 0.3f, 0.4f); // 머티리얼: 빨간색
		}
		else if (ext == ".hlsl" || ext == ".glsl")
		{
			icon = ICON_FA_CODE;
			typeColor = ImVec4(0.7f, 0.3f, 0.9f, 0.4f); // 셰이더: 보라색
		}
		else if (ext == ".csv" || ext == ".json")
		{
			icon = ICON_FA_FILE_LINES;
			typeColor = ImVec4(0.6f, 0.6f, 0.6f, 0.4f); // 데이터: 회색
		}
	}

	ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// 1. 텍스처가 없을 때만 배경 색상 + 텍스트 아이콘 렌더링 (Fallback)
	if (!thumbnailTexID)
	{
		drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + m_ThumbnailSize, cursorPos.y + m_ThumbnailSize), ImGui::GetColorU32(typeColor), 8.0f); // 8.0f는 라운드 처리

		ImFont* font = ImGui::GetFont();
		float iconFontSize = m_ThumbnailSize * 0.85f; // 아이콘 크기를 85%로 꽉 차게
		ImVec2 iconSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, icon);
		ImVec2 iconPos = ImVec2(cursorPos.x + (m_ThumbnailSize - iconSize.x) * 0.5f, cursorPos.y + (m_ThumbnailSize - iconSize.y) * 0.5f);
		drawList->AddText(font, iconFontSize, iconPos, ImGui::GetColorU32(ImGuiCol_Text), icon);
	}

	// 2. 투명 버튼 스타일 지정
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));

	string buttonId = "##Btn_" + directoryEntry.path().filename().string();

	// 3. 텍스처 존재 여부에 따라 ImageButton 또는 일반 투명 Button 렌더링
	if (thumbnailTexID)
	{
		ImGui::ImageButton(buttonId.c_str(), (ImTextureID)thumbnailTexID, { m_ThumbnailSize, m_ThumbnailSize });
	}
	else
	{
		ImGui::Button(buttonId.c_str(), { m_ThumbnailSize, m_ThumbnailSize });
	}

	ImGui::PopStyleColor(3);
}

void ContentBrowserPanel::GridItemContextMenu(const filesystem::path& path)
{
	string filenameString = path.filename().string();
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem(ICON_FA_PEN " Rename", "F2"))
		{
			m_RenamingPath = path;
			strncpy(m_RenameBuffer, filenameString.c_str(), sizeof(m_RenameBuffer));
		}

		if (ImGui::MenuItem(ICON_FA_COPY " Copy Path", "Ctrl+C"))
		{
			ImGui::SetClipboardText(path.string().c_str());
		}

		ImGui::Separator();

		if (ImGui::MenuItem(ICON_FA_TRASH " Delete"))
		{
			// 삭제 로직
		}

		ImGui::EndPopup();
	}

	if (ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_F2))
	{
		m_RenamingPath = path;
		strncpy(m_RenameBuffer, filenameString.c_str(), sizeof(m_RenameBuffer));
	}
}

void ContentBrowserPanel::DragAndDropTarget(const filesystem::path& path)
{
	if (ImGui::BeginDragDropSource())
	{
		const wchar_t* itemPath = path.c_str();
		// "CONTENT_BROWSER_ITEM" 이라는 이름으로 경로 데이터 전달
		ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));

		ImGui::Text("%s", path.filename().string().c_str());
		ImGui::EndDragDropSource();
	}
}

#pragma endregion

#pragma region Helper
void ContentBrowserPanel::DrawRename(const filesystem::path& path)
{
	// 입력창 너비 설정 (그리드일 때는 썸네일 크기, 트리일 때는 가변)
	float width = (m_CurrentDirectory == path.parent_path()) ? m_ThumbnailSize : ImGui::GetContentRegionAvail().x * 0.8f;
	ImGui::SetNextItemWidth(width);

	if (ImGui::InputText("##RenameInput", m_RenameBuffer, sizeof(m_RenameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
	{
		filesystem::path newPath = path.parent_path() / m_RenameBuffer;
		if (m_RenameBuffer[0] != '\0' && !filesystem::exists(newPath))
		{
			try {
				filesystem::rename(path, newPath);
				if (m_CurrentDirectory == path) m_CurrentDirectory = newPath;
			}
			catch (...) {}
		}
		m_RenamingPath.clear();
	}

	// 처음 나타날 때 포커스 강제
	if (ImGui::IsItemVisible() && !ImGui::IsAnyItemActive())
		ImGui::SetKeyboardFocusHere(-1);

	// ESC를 누르거나, 입력창 활성 상태에서 다른 곳을 클릭하면 취소
	if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit())
	{
		m_RenamingPath.clear();
	}
	else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
	{
		m_RenamingPath.clear();
	}
}
void* ContentBrowserPanel::LoadThumbnail(const filesystem::path& path)
{
	string pathStr = path.string();

	if(m_ThumbnailCache.find(pathStr) != m_ThumbnailCache.end())
	{
		return m_ThumbnailCache[pathStr];
	}

	int32 width, height, channels;

	stbi_uc* data = stbi_load(pathStr.c_str(), &width, &height, &channels, 4);

	if (data)
	{
		RHITexture* rhiTexture = Renderer::Get().GetRHI()->CreateTextureFromMemory(data, width, height, 1, 1, 4);
		stbi_image_free(data);

		if (rhiTexture)
		{
			m_ThumbnailTextures[pathStr] = rhiTexture; // take ownership
			m_ThumbnailCache[pathStr] = (void*)(ImTextureID)(size_t)(rhiTexture->GetNativeHandle());
			return m_ThumbnailCache[pathStr];
		}
	}

	m_ThumbnailCache[pathStr] = nullptr;
	return nullptr;
}
#pragma endregion

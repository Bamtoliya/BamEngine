#pragma once
#include "ContentBrowserPanel.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

using namespace std;

static bool StringContainsCaseInsensitive(const string& str, const string& subStr)
{
	if (subStr.empty()) return true;

	std::wstring wStr = StrToWStr(str);
	std::wstring wSubStr = StrToWStr(subStr);

	auto it = std::search(
		wStr.begin(), wStr.end(),
		wSubStr.begin(), wSubStr.end(),
		[](wchar_t ch1, wchar_t ch2) {
			// CharUpperW를 사용하여 다국어 문자도 안전하게 대문자로 변환하여 비교
			return CharUpperW((LPWSTR)(INT_PTR)ch1) == CharUpperW((LPWSTR)(INT_PTR)ch2);
		}
	);
	return (it != wStr.end());
}

static bool PassesFilter(const filesystem::path& path, uint32 filterMask)
{
	if (filterMask == 0) return true; // 아무것도 선택하지 않거나 "All"인 경우 모두 표시
	if (filesystem::is_directory(path)) return true; // 폴더는 무조건 표시

	string ext = path.extension().string();
	for (auto& c : ext) c = tolower(c); // 소문자로 변환

	// 선택된 각 카테고리의 비트가 켜져있는지 확인하고 해당 확장자면 true 반환
	if ((filterMask & (1 << 0)) && (ext == ".png" || ext == ".jpg" || ext == ".tga")) return true; // Texture
	if ((filterMask & (1 << 1)) && (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".blend")) return true; // Model
	if ((filterMask & (1 << 2)) && (ext == ".mat")) return true; // Material
	if ((filterMask & (1 << 3)) && (ext == ".hlsl" || ext == ".glsl")) return true; // Shader
	if ((filterMask & (1 << 4)) && (ext == ".csv" || ext == ".json")) return true; // Data

	return false;
}

EResult ContentBrowserPanel::Initialize(void* arg)
{
	m_Name = L"Content Browser";
	filesystem::path resourcePath = filesystem::current_path();
	resourcePath.append("Resources");
	m_RootPath = resourcePath;
	m_CurrentDirectory = m_RootPath;
	m_LastDirectory = m_CurrentDirectory;
	m_NeedToExpandTree = true;
	memset(m_SearchBuffer, 0, sizeof(m_SearchBuffer));
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

void ContentBrowserPanel::ProcessEvent(const SDL_Event* event)
{
	if(event->type == SDL_EVENT_DROP_FILE)
	{
		const char* droppedFile = event->drop.data;
		if(droppedFile)
		{
			vector<string> droppedFiles = { droppedFile };
			OnExteranalDropped(droppedFiles);
		}
	}
}

void ContentBrowserPanel::Free()
{
	for (auto& [path, texture] : m_ThumbnailTextures)
	{
		if (texture) texture->Release();
	}
	m_ThumbnailTextures.clear();
	m_ThumbnailCache.clear();
	Safe_Delete(m_FileListener);
	Safe_Delete(m_FileWatcher);
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
	ImGui::SameLine();
	ContentFilter();
	ImGui::SameLine();
	bool enterPressed = SearchBar();
	ImGui::Separator();

	if (ImGui::BeginChild("GridArea", ImVec2(0, 0), false))
	{
		if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl)
		{
			float wheel = ImGui::GetIO().MouseWheel;
			if (wheel != 0.0f)
			{
				m_ThumbnailSize += wheel * 10.0f; // 스크롤 민감도

				// 크기 한계 지정
				if (m_ThumbnailSize < 32.0f) m_ThumbnailSize = 32.0f;
				if (m_ThumbnailSize > 256.0f) m_ThumbnailSize = 256.0f;
			}
		}

		GridViewContextMenu(m_CurrentDirectory);
		DrawGrid(enterPressed);
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

#pragma region Header Bar
void ContentBrowserPanel::BackButton()
{
	bool isRoot = (m_CurrentDirectory == m_RootPath);

	if (isRoot) ImGui::BeginDisabled();

	if (ImGui::Button(ICON_FA_ARROW_LEFT_LONG))
	{
		m_CurrentDirectory = m_CurrentDirectory.parent_path();
		memset(m_SearchBuffer, 0, sizeof(m_SearchBuffer));
	}

	if (isRoot) ImGui::EndDisabled();
}

void ContentBrowserPanel::AddressBar()
{
	float availWidth = ImGui::GetContentRegionAvail().x - 280.f;
	if (availWidth < 100.f) availWidth = 100.f;

	ImGui::BeginChild("##AddressBarChild", ImVec2(availWidth, ImGui::GetFrameHeight()), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	// 브레드크럼 버튼의 기본 스타일을 투명하게 설정
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

	// 현재 경로를 분해하여 노드 리스트 생성
	std::vector<std::filesystem::path> nodes;
	std::filesystem::path current = m_CurrentDirectory;
	while (current != m_RootPath && !current.empty() && current != current.parent_path())
	{
		nodes.push_back(current);
		current = current.parent_path();
	}
	nodes.push_back(m_RootPath);
	std::reverse(nodes.begin(), nodes.end()); // Root부터 시작하도록 순서 뒤집기

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		std::string name = (i == 0) ? "Assets" : nodes[i].filename().string();

		// 경로의 폴더 이름을 클릭하면 해당 경로로 즉시 이동
		if (ImGui::Button(name.c_str()))
		{
			m_CurrentDirectory = nodes[i];
			memset(m_SearchBuffer, 0, sizeof(m_SearchBuffer));
			m_LastSearchString = "";
			m_SearchResults.clear();
		}

		// 마지막 노드가 아니라면 구분자(>) 추가
		if (i < nodes.size() - 1)
		{
			ImGui::SameLine();
			ImGui::TextUnformatted(">");
			ImGui::SameLine();
		}
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
	ImGui::EndChild();
}

bool ContentBrowserPanel::SearchBar()
{
	ImGui::SetNextItemWidth(150.f);
	return ImGui::InputTextWithHint("##SearchBar", ICON_FA_MAGNIFYING_GLASS " Search...", m_SearchBuffer, sizeof(m_SearchBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
}

void ContentBrowserPanel::ContentFilter()
{
	const char* filterNames[] = { "Texture", "Model", "Material", "Shader", "Data" };
	int filterCount = IM_ARRAYSIZE(filterNames);

	// 콤보박스 타이틀 설정 로직
	std::string previewText = "All";
	if (m_FilterMask != 0)
	{
		int activeCount = 0;
		std::string activeNames = "";
		for (int i = 0; i < filterCount; ++i)
		{
			if (m_FilterMask & (1 << i))
			{
				if (activeCount > 0) activeNames += ", ";
				activeNames += filterNames[i];
				activeCount++;
			}
		}

		if (activeCount == 1)
			previewText = activeNames;
		else if (activeCount > 1)
			previewText = "Multiple (" + std::to_string(activeCount) + ")";
	}

	ImGui::SetNextItemWidth(100.f);
	if (ImGui::BeginCombo("##Filter", previewText.c_str()))
	{
		// "All" 항목 (마스크 초기화)
		bool allSelected = (m_FilterMask == 0);
		if (ImGui::Selectable("All", allSelected))
		{
			m_FilterMask = 0;
		}
		ImGui::Separator();

		// 각 필터별 체크박스
		for (int i = 0; i < filterCount; ++i)
		{
			bool isSelected = (m_FilterMask & (1 << i)) != 0;
			if (ImGui::Checkbox(filterNames[i], &isSelected))
			{
				if (isSelected)
					m_FilterMask |= (1 << i);
				else
					m_FilterMask &= ~(1 << i);
			}
		}
		ImGui::EndCombo();
	}
}
#pragma endregion

void ContentBrowserPanel::DrawGridItem(const filesystem::directory_entry& directoryEntry)
{
	const auto& path = directoryEntry.path();
	auto relativePath = filesystem::relative(path, m_RootPath);
	string filenameString = relativePath.filename().string();

	ImGui::PushID(path.string().c_str());

	DrawThumbnail(directoryEntry);
	GridItemTooltip(directoryEntry);
	GridItemContextMenu(path);
	DragAndDropTarget(relativePath);


	// 인터랙션: 폴더 진입
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
	{
		if (directoryEntry.is_directory())
		{
			m_CurrentDirectory /= path.filename();
			memset(m_SearchBuffer, 0, sizeof(m_SearchBuffer));
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

void ContentBrowserPanel::GridItemTooltip(const filesystem::directory_entry& directoryEntry)
{
	if (ImGui::IsItemHovered())
	{
		filesystem::path path = directoryEntry.path();
		string filenameString = path.filename().string();
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(filenameString.c_str());
		ImGui::Separator();

		if (directoryEntry.is_directory())
		{
			ImGui::Text("Type: Folder");
		}
		else
		{
			string ext = path.extension().string();
			for (auto& c : ext) c = tolower(c);
			ImGui::Text("Type: %s File", ext.c_str());

			// 안전하게 파일 용량 가져오기
			try
			{
				uintmax_t fileSize = filesystem::file_size(path);
				if (fileSize < 1024)
					ImGui::Text("Size: %ju Bytes", fileSize);
				else if (fileSize < 1024 * 1024)
					ImGui::Text("Size: %ju KB", fileSize / 1024);
				else
					ImGui::Text("Size: %.2f MB", (float)fileSize / (1024.0f * 1024.0f));
			}
			catch (...) {}
		}
		ImGui::EndTooltip();
	}
}

void ContentBrowserPanel::DrawGrid(bool enterPressed)
{
	f32 cellSize = m_ThumbnailSize + m_Padding;
	f32 panelWidth = ImGui::GetContentRegionAvail().x;
	int32 columnCount = static_cast<int>(panelWidth / cellSize);
	if (columnCount < 1) columnCount = 1;

	bool needsCacheRefresh = false;

	if (m_LastWatchedDirectory != m_CurrentDirectory)
	{
		m_LastWatchedDirectory = m_CurrentDirectory;
		needsCacheRefresh = true;
		if (!m_FileWatcher)
		{
			m_FileWatcher = new efsw::FileWatcher();
			m_FileListener = new ContentBrowserFileListener(&m_NeedsCacheRefresh);
		}

		if(m_WatchID > 0)
			m_FileWatcher->removeWatch(m_WatchID);

		const string& watchPath = m_CurrentDirectory.string();
		m_WatchID = m_FileWatcher->addWatch(watchPath, m_FileListener, false);
	}
	else if (m_NeedsCacheRefresh.exchange(false))
	{
		needsCacheRefresh = true;
	}

	if(needsCacheRefresh)
	{
		m_CachedEntries.clear();
		for (auto& entry : filesystem::directory_iterator(m_CurrentDirectory))
		{
			m_CachedEntries.push_back(entry);
		}
	}

	if (ImGui::BeginTable("ContentBrowserTable", columnCount))
	{
		string searchStr(m_SearchBuffer);
		bool isSearching = !searchStr.empty();
		bool searchCleared = (m_SearchBuffer[0] == '\0' && !m_LastSearchString.empty());

		if (enterPressed)
		{
			m_LastSearchString = searchStr; // 검색어 업데이트
			m_SearchResults.clear();

			if (isSearching)
			{
				for (auto& directoryEntry : filesystem::recursive_directory_iterator(m_RootPath, filesystem::directory_options::skip_permission_denied))
				{
					string filenameString = directoryEntry.path().filename().string();
					if (StringContainsCaseInsensitive(filenameString, searchStr))
					{
						m_SearchResults.push_back(directoryEntry);
					}
				}
			}
		}

		if (isSearching && !m_SearchResults.empty())
		{
			for (auto& directoryEntry : m_SearchResults)
			{
				if (!PassesFilter(directoryEntry.path(), m_FilterMask)) continue;
				ImGui::TableNextColumn();
				DrawGridItem(directoryEntry);
			}
		}
		else
		{
			for (auto& directoryEntry : m_CachedEntries)
			{
				if (!PassesFilter(directoryEntry.path(), m_FilterMask)) continue;
				ImGui::TableNextColumn();
				DrawGridItem(directoryEntry);
			}
			m_SearchResults.clear();
		}
		ImGui::EndTable();
	}
}

void ContentBrowserPanel::DrawThumbnail(const filesystem::directory_entry& directoryEntry)
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
			thumbnailTexID = LoadModelThumbnail(directoryEntry.path());
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
void ContentBrowserPanel::OnExteranalDropped(const vector<string>& droppedFiles)
{
	for (const string& filepathStr : droppedFiles)
	{
		filesystem::path sourcePath(filepathStr);

		if (filesystem::exists(sourcePath))
		{
			filesystem::path destinationPath = m_CurrentDirectory / sourcePath.filename();
			try
			{
				// recursive 옵션을 주어 폴더일 경우 내부 파일들까지 통째로 복사
				// overwrite_existing 옵션을 주어 이름이 겹치면 덮어쓰도록 처리
				auto copyOptions = filesystem::copy_options::recursive | filesystem::copy_options::overwrite_existing;
				filesystem::copy(sourcePath, destinationPath, copyOptions);
			}
			catch (const filesystem::filesystem_error& e)
			{
				// 복사 실패 처리
			}
		}
	}
}
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
void* ContentBrowserPanel::LoadModelThumbnail(const filesystem::path& path)
{
	string pathStr = path.string();

	if (m_ThumbnailCache.find(pathStr) != m_ThumbnailCache.end())
	{
		return m_ThumbnailCache[pathStr];
	}

	bool comInitialized = SUCCEEDED(CoInitialize(NULL));

	// [수정점 1] SHCreateItemFromParsingName은 절대 경로를 요구합니다.
	filesystem::path absolutePath = filesystem::absolute(path);
	wstring wPathStr = absolutePath.wstring();

	IShellItemImageFactory* imageFactory = nullptr;
	HRESULT hr = SHCreateItemFromParsingName(wPathStr.c_str(), nullptr, IID_PPV_ARGS(&imageFactory));

	if (SUCCEEDED(hr) && imageFactory)
	{
		HBITMAP hBitmap;
		SIZE size = { 256, 256 }; // 원하는 썸네일 크기

		// [수정점 2] SIIGBF_ICONONLY를 제거하고 SIIGBF_RESIZETOFIT만 사용합니다.
		hr = imageFactory->GetImage(size, SIIGBF_RESIZETOFIT, &hBitmap);

		imageFactory->Release();
		if (SUCCEEDED(hr))
		{
			BITMAP bm;
			GetObject(hBitmap, sizeof(bm), &bm);

			int width = bm.bmWidth;
			int height = bm.bmHeight;

			BITMAPINFO bi = { 0 };
			bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bi.bmiHeader.biWidth = width;
			bi.bmiHeader.biHeight = -height; // Top-down 방식으로 읽기
			bi.bmiHeader.biPlanes = 1;
			bi.bmiHeader.biBitCount = 32;
			bi.bmiHeader.biCompression = BI_RGB;

			HDC hdc = GetDC(NULL);
			std::vector<uint8_t> pixels(width * height * 4);
			GetDIBits(hdc, hBitmap, 0, height, pixels.data(), &bi, DIB_RGB_COLORS);
			ReleaseDC(NULL, hdc);

			// Windows HBITMAP은 기본적으로 BGRA 순서이므로 RGBA로 스왑
			for (size_t i = 0; i < pixels.size(); i += 4)
			{
				uint8_t b = pixels[i];
				uint8_t r = pixels[i + 2];
				pixels[i] = r;
				pixels[i + 2] = b;

				// 윈도우 썸네일은 포맷에 따라 알파가 0으로 올 수 있으므로 불투명(255) 강제 처리
				pixels[i + 3] = 255;
			}

			DeleteObject(hBitmap);

			RHITexture* rhiTexture = Renderer::Get().GetRHI()->CreateTextureFromMemory(pixels.data(), width, height, 1, 1, 4);
			if (rhiTexture)
			{
				m_ThumbnailTextures[pathStr] = rhiTexture;
				m_ThumbnailCache[pathStr] = (void*)(ImTextureID)(size_t)(rhiTexture->GetNativeHandle());
				if (comInitialized) CoUninitialize();
				return m_ThumbnailCache[pathStr];
			}
		}
	}

	if (comInitialized)
	{
		CoUninitialize();
	}

	m_ThumbnailCache[pathStr] = nullptr;
	return nullptr;
}
#pragma endregion

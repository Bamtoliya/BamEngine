#pragma once
#include "ContentBrowserPanel.h"
#include "ContentBrowserGridItem.h"
#include "ContentBrowserTreeItem.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "LocalizationManager.h"
#include "AssetManager.h"
#include "Archives.h"

using namespace std;

#pragma region ContentBrowserFileListner
void ContentBrowserFileListener::handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename, efsw::Action action, std::string oldFilename)
{
	if (action == efsw::Action::Modified || action == efsw::Action::Add || action == efsw::Action::Delete || action == efsw::Action::Moved)
	{
		filesystem::path fullPath = dir;
		fullPath.append(filename);
		AssetCache::Get().ClearCache(fullPath);
	}

	if (m_RefreshFlag)
	{
		*m_RefreshFlag = true; // 메인 스레드에 캐시 갱신이 필요함을 알림
	}
}
#pragma endregion

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

static void ResolveConflict(const FileDropConflict& conflict, int action)
{
	try {
		if (action == 1) // 덮어쓰기
		{
			AssetManager::Get().ImportAsync(conflict.Source, conflict.Dest);
		}
		else if (action == 2) // 복사본 생성 (Rename)
		{
			std::string stem = conflict.Dest.stem().string();
			std::string ext = conflict.Dest.extension().string();
			std::filesystem::path newDest;
			int suffix = 1;

			// filename_copy1, filename_copy2 형식으로 중복 안 될 때까지 탐색
			do {
				newDest = conflict.Dest.parent_path() / (stem + "_copy" + std::to_string(suffix++) + ext);
			} while (std::filesystem::exists(newDest));

			AssetManager::Get().ImportAsync(conflict.Source, newDest);
		}
		// action == 3 (건너뛰기) 일 경우 아무 작업도 하지 않음
	}
	catch (...) {}
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
	AssetManager::Get().GetAsyncDelegate().AddLambda([this]() {
		RequestRefresh();
		});
	return EResult::Success;
}

void ContentBrowserPanel::Draw()
{
	// AssetManager가 처리 중인 비동기 작업이 있다면 UI에 로딩 상태를 표시
	size_t activeTasks = AssetManager::Get().GetActiveTaskCount();
	if (activeTasks > 0)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), ICON_FA_ARROW_ROTATE_RIGHT " Importing %d Asset(s) in background...", (int)activeTasks);
		ImGui::Separator();
	}

	if (!m_Open) return;

	ImGui::Begin(WStrToStr(m_Name).c_str(), &m_Open);

	FileConfilictPopup();

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
	Safe_Delete(m_FileListener);
	Safe_Delete(m_FileWatcher);
}

#pragma region TreeView
void ContentBrowserPanel::DrawDirectoryTree(const filesystem::path& path)
{
	bool hasChildDirectory = false;
	std::error_code ec;
	auto dirIt = filesystem::directory_iterator(path, ec);
	if (!ec)
	{
		for (auto& directoryEntry : filesystem::directory_iterator(path))
		{
			if (directoryEntry.is_directory())
			{
				hasChildDirectory = true;
				break;
			}
		}
	}

	bool isSelected = (m_CurrentDirectory == path);

	// 분리한 TreeItem 클래스에 노드 렌더링 위임
	bool shouldDrawChildren = ContentBrowserTreeItem::DrawNode(
		path,
		m_RootPath,
		hasChildDirectory,
		isSelected,
		m_NeedToExpandTree,
		m_RenamingPath,
		m_RenameBuffer,
		sizeof(m_RenameBuffer),
		m_CurrentDirectory
	);

	// 노드가 열렸고 자식 디렉토리가 있다면 재귀 호출
	if (shouldDrawChildren)
	{
		std::error_code childEc;
		auto childIt = std::filesystem::directory_iterator(path, childEc);
		if (!childEc)
		{
			for (auto& entry : childIt)
			{
				if (entry.is_directory())
					DrawDirectoryTree(entry.path());
			}
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
			TODO("Tree View Folder 삭제 기능 구현하기")
		}

		ImGui::EndPopup();
	}

	if (ImGui::IsItemHovered() && KEY_DOWN(EKeyCode::F2))
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
	RefreshButton();
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
		if (ImGui::BeginMenu(ICON_FA_PLUS " Create"))
		{
			if (ImGui::MenuItem(ICON_FA_FOLDER " New Folder"))
			{
				filesystem::path newFolderPath = m_CurrentDirectory / "NewFolder";
				int suffix = 1;
				while (filesystem::exists(newFolderPath))
				{
					newFolderPath = m_CurrentDirectory / ("NewFolder_" + std::to_string(suffix++));
				}
				filesystem::create_directory(newFolderPath);
			}

			if (ImGui::MenuItem(ICON_FA_GLOBE " Create Empty Material"))
			{
				CreateEmptyMaterial(m_CurrentDirectory);
			}

			ImGui::EndMenu();
		}
		

		ImGui::Separator();

		if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Show in Explorer"))
		{
			// Windows 전용 명령 (ShellExecute)
			ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
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

void ContentBrowserPanel::RefreshButton()
{
	if(ImGui::Button(ICON_FA_ARROW_ROTATE_RIGHT))
	{
		RequestRefresh();
	}
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
		name += "###" + std::to_string(static_cast<int>(i));

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

	void* thumbnailTexID = nullptr;
	if (!directoryEntry.is_directory())
	{
		thumbnailTexID = AssetCache::Get().GetThumbnail(path);
		string ext = path.extension().string();
	}

	ContentBrowserGridItem::Draw(
		directoryEntry,
		m_RootPath,
		thumbnailTexID,
		m_ThumbnailSize,
		m_Padding,
		m_RenamingPath,
		m_RenameBuffer,
		sizeof(m_RenameBuffer),
		m_CurrentDirectory,
		m_SearchBuffer
	);
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
			m_FileWatcher->watch();
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
		std::error_code ec;
		if (std::filesystem::exists(m_CurrentDirectory, ec))
		{
			for (auto& directoryEntry : filesystem::directory_iterator(m_CurrentDirectory, ec))
			{
				m_CachedEntries.push_back(directoryEntry);
			}
		}
		else
		{
			// 현재 디렉토리가 외부에서 통째로 날아갔다면 루트 에셋 폴더로 강제 복귀
			m_CurrentDirectory = m_RootPath;
			m_LastWatchedDirectory = ""; // 다음 프레임에 Watcher 재설정을 유도
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

void ContentBrowserPanel::FileConfilictPopup()
{
	LocalizationManager& localizationManager = LocalizationManager::Get();
	string popupTitle = localizationManager.GetText("ContentBrowser.FileConflictPopup");
	if(m_OpenConflictPopup)
	{
		ImGui::OpenPopup(popupTitle.c_str());
		m_OpenConflictPopup = false;
	}

	if (ImGui::BeginPopupModal(popupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if (m_PendingConflicts.empty())
		{
			ImGui::CloseCurrentPopup();
		}
		else
		{
			auto& conflict = m_PendingConflicts.front();
			
			ImGui::Text("%s", localizationManager.GetText("ContentBrowser.FileConflictMessage").c_str());
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", conflict.Dest.filename().string().c_str());
			ImGui::Separator();
			if (m_PendingConflicts.size() > 1)
			{
				ImGui::Checkbox(localizationManager.GetText("ContentBrowser.ApplyToAll").c_str(), &m_ApplyToAllConflicts);
			}

			auto processConflict = [&](int32 action)
				{
					if(m_ApplyToAllConflicts)
					{
						for (const auto& conflict : m_PendingConflicts)
						{
							ResolveConflict(conflict, action);
						}
						m_PendingConflicts.clear();
					}
					else
					{
						const auto& conflict = m_PendingConflicts.front();
						ResolveConflict(conflict, action);
						m_PendingConflicts.erase(m_PendingConflicts.begin());
						m_NeedsCacheRefresh = true;
					}

					if (m_PendingConflicts.empty())
					{
						m_ApplyToAllConflicts = false;
						ImGui::CloseCurrentPopup();
					}
				};

			if (ImGui::Button(localizationManager.GetText("ContentBrowser.Overwrite").c_str())) processConflict(1);
			ImGui::SameLine();
			if (ImGui::Button(localizationManager.GetText("ContentBrowser.CreateCopy").c_str())) processConflict(2);
			ImGui::SameLine();
			if (ImGui::Button(localizationManager.GetText("ContentBrowser.Skip").c_str())) ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
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

			std::error_code ec;
			if (filesystem::exists(destinationPath, ec) && filesystem::equivalent(sourcePath, destinationPath, ec))
			{
				AssetManager::Get().ImportAsync(sourcePath, destinationPath);
				continue;
			}

			bool isConflict = false;
			std::string srcStem = sourcePath.stem().string();
			std::string srcExt = sourcePath.extension().string();
			for (auto& c : srcStem) c = tolower(c);
			for (auto& c : srcExt) c = tolower(c);

			if (filesystem::exists(m_CurrentDirectory, ec))
			{
				for (const auto& entry : filesystem::directory_iterator(m_CurrentDirectory, ec))
				{
					std::string existingStem = entry.path().stem().string();
					std::string existingExt = entry.path().extension().string();
					for (auto& c : existingStem) c = tolower(c);
					for (auto& c : existingExt) c = tolower(c);

					if (srcStem == existingStem && srcExt == existingExt)
					{
						isConflict = true;
						destinationPath = entry.path();
						break;
					}
				}
			}

			if (isConflict)
			{
				FileDropConflict conflict{ sourcePath, destinationPath };
				m_PendingConflicts.push_back(conflict);
				m_OpenConflictPopup = true;
			}
			else
			{
				AssetManager::Get().Import(sourcePath, destinationPath);
			}
		}
	}
}
void ContentBrowserPanel::CreateEmptyMaterial(const filesystem::path& path)
{
	filesystem::path outputPath = path;
	outputPath += "\\NewMaterial.bammat";
	JsonArchive archive(EArchiveMode::Write);
	Material* material = Engine::Material::CreateEmpty();
	material->Serialize(archive);
	archive.SaveToFile(outputPath.string());
	ResourceManager::Get().DestroyResource(material);
}
#pragma endregion

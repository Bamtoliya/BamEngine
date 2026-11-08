#pragma once
#include "InputManager.h"
#include "ContentBrowserPanel.h"
#include "ContentBrowserGridItem.h"
#include "ContentBrowserTreeItem.h"
#include "Editor_Macros.h"
#include "EditorSettings.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "LocalizationManager.h"
#include "AssetManager.h"
#include "ImGuiManager.h"
#include "EditorSettingsService.h"
#include "ShortcutManager.h"
#include "ModelImportModal.h"
#include "NotificationManager.h"
#include <algorithm>
#include <utility>

using namespace std;

#pragma region ContentBrowserFileListner
void ContentBrowserFileListener::handleFileAction(efsw::WatchID watchid, const std::string& dir,
	const std::string& filename, efsw::Action action, std::string oldFilename)
{
	if (action != efsw::Action::Modified && action != efsw::Action::Add &&
		action != efsw::Action::Delete && action != efsw::Action::Moved)
	{
		return;
	}

	try
	{
		// C++20 표준 방식: reinterpret_cast<const char8_t*>를 사용해 UTF-8 문자열임을 명시하여 path 생성
		const filesystem::path directory(reinterpret_cast<const char8_t*>(dir.c_str()));
		auto& cache = AssetCache::Get();

		const filesystem::path filePath = directory / reinterpret_cast<const char8_t*>(filename.c_str());
		cache.RequestInvalidate(filePath);

		if (action == efsw::Action::Moved && !oldFilename.empty())
		{
			const filesystem::path oldFilePath = directory / reinterpret_cast<const char8_t*>(oldFilename.c_str());
			cache.RequestInvalidate(oldFilePath);
		}

		if (m_RefreshFlag)
			m_RefreshFlag->store(true);
	}
	catch (const std::exception& e)
	{
		// 백그라운드 파일 감시자 스레드 예외로 인한 전체 프로세스 크래시 방어
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

static bool PassesFilter(const filesystem::directory_entry& entry, uint32 filterMask)
{
	if (filterMask == 0) return true; // 아무것도 선택하지 않거나 "All"인 경우 모두 표시
	if (entry.is_directory()) return true; // 폴더는 무조건 표시
	string filename = entry.path().filename().string();
	for (auto& c : filename) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	string ext = entry.path().extension().string();
	for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	const bool isTexture = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
		ext == ".bmp" || ext == ".dds" || ext == ".hdr" || ext == ".bamtex" || ext == ".bamsprite" ||
		ext == ".bamspriteanim" || ext == ".spriteanim" ||
		filename.ends_with(".bamtex.json") || filename.ends_with(".bamsprite.json") ||
		filename.ends_with(".bamspriteanim.json") || filename.ends_with(".spriteanim.json"));
	const bool isModel = (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" ||
		ext == ".blend" || ext == ".bammodel" || ext == ".bammesh" || ext == ".bamanim" || ext == ".anim" ||
		ext == ".bamskel" || ext == ".bamanimator" || ext == ".animator" ||
		filename.ends_with(".bammodel.json") || filename.ends_with(".bammesh.json") ||
		filename.ends_with(".bamanim.json") || filename.ends_with(".anim.json") ||
		filename.ends_with(".bamskel.json") || filename.ends_with(".bamanimator.json") || filename.ends_with(".animator.json"));
	const bool isMaterial = (ext == ".mat" || ext == ".bammat" || ext == ".bammatinst" || ext == ".materialinstance" ||
		filename.ends_with(".bammat.json") || filename.ends_with(".bammatinst.json"));
	const bool isShader = (ext == ".hlsl" || ext == ".hlsli" || ext == ".glsl" ||
		ext == ".vert" || ext == ".frag" || ext == ".comp" || ext == ".cso" || ext == ".spv" ||
		ext == ".bamshader" || filename.ends_with(".bamshader.json"));
	const bool isData = (ext == ".csv" || ext == ".beve" || ext == ".ini" || ext == ".cfg" ||
		ext == ".config" || ext == ".toml" || ext == ".yaml" || ext == ".yml" || ext == ".xml" ||
		ext == ".bamscript" || ext == ".bamgraph" || ext == ".bamprefab" || ext == ".prefab" || ext == ".bamscene" ||
		(ext == ".json" && !isTexture && !isModel && !isMaterial && !isShader));
	if ((filterMask & (1 << 0)) && isTexture)  return true; // Texture / Sprite / SpriteAnim
	if ((filterMask & (1 << 1)) && isModel)    return true; // Model / Mesh / Anim / Animator
	if ((filterMask & (1 << 2)) && isMaterial) return true; // Material / MaterialInstance
	if ((filterMask & (1 << 3)) && isShader)   return true; // Shader
	if ((filterMask & (1 << 4)) && isData)     return true; // Data / Scene / Prefab / Script / Config
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

EResult ContentBrowserPanel::OnInitialize(void*)
{
	m_Name = L"Content Browser"; m_DisplayNameKey = "ContentBrowser.Title";
	const auto projectRoot = EditorSettingsService::IsCreated() ? EditorSettingsService::Get().GetProjectRoot() : Engine::PlatformPath::GetExecutableDirectory();
	m_RootPath = Engine::PlatformPath::GetProjectAssetRoot();
	if (m_RootPath.empty())
		m_RootPath = projectRoot / "Resources";
	m_EngineRootPath = Engine::PlatformPath::GetEngineResourceRoot();
	if (m_EngineRootPath.empty())
		m_EngineRootPath = m_RootPath;
	m_CurrentDirectory = m_RootPath;
	m_LastDirectory = m_CurrentDirectory;
	m_NeedToExpandTree = true;
	memset(m_SearchBuffer, 0, sizeof(m_SearchBuffer));
	m_FileWatcher = new efsw::FileWatcher();
	m_FileListener = new ContentBrowserFileListener(&m_NeedsCacheRefresh);
	m_WatchID = m_FileWatcher->addWatch(m_RootPath.string(), m_FileListener, true);
	std::error_code ec;
	if (m_EngineRootPath != m_RootPath && std::filesystem::is_directory(m_EngineRootPath, ec))
		m_EngineWatchID = m_FileWatcher->addWatch(m_EngineRootPath.string(), m_FileListener, true);
	m_FileWatcher->watch();
	m_AsyncCompletionHandle = AssetManager::Get().OnAsyncCompleted().AddLambda([this]()
		{
			RequestRefresh();
		});
	m_ImportPreviewWidget.Initialize(L"ImportModalPreview", 380, 380, true);
	return EResult::Success;
}

void ContentBrowserPanel::SetShowEngineContent(bool show)
{
	if (m_ShowEngineContent == show)
		return;
	m_ShowEngineContent = show;
	m_CachedSubdirectories.clear();
	if (!m_ShowEngineContent && Engine::PlatformPath::IsReadOnlyTarget(m_CurrentDirectory))
	{
		m_CurrentDirectory = m_RootPath;
		m_NeedToExpandTree = true;
	}
	RequestRefresh();
}

const filesystem::path& ContentBrowserPanel::GetActiveRootForPath(const filesystem::path& path) const
{
	if (!m_EngineRootPath.empty() && m_EngineRootPath != m_RootPath)
	{
		const auto normPath = Engine::PlatformPath::Normalize(path);
		const auto normEngine = Engine::PlatformPath::Normalize(m_EngineRootPath);
		if (normPath == normEngine)
			return m_EngineRootPath;
		const auto rel = normPath.lexically_relative(normEngine);
		if (!rel.empty() && !rel.is_absolute() && !rel.native().starts_with(L".."))
			return m_EngineRootPath;
	}
	return m_RootPath;
}

void ContentBrowserPanel::Update(f32 dt)
{
	if (m_IsImportModalActive)
	{
		m_ImportPreviewWidget.BeginFrame(dt);
		m_ImportPreviewWidget.Submit(dt);
	}
}

void ContentBrowserPanel::Draw()
{
	m_Focused = false;
	m_Hovered = false;
	m_RenameTargetPath.clear();

	if (!m_Open)
		return;

	if (m_FocusRequested)
		ImGui::SetNextWindowFocus();

	const string windowTitle = LOCAL("ContentBrowser.Title") + "###ContentBrowserPanel";
	if (!BeginPanelWindow(windowTitle.c_str(), m_Open))
	{
		ImGui::End();
		return;
	}

	if (m_FocusRequested)
	{
		ImGui::SetWindowFocus();
		m_FocusRequested = false;
	}

	m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

	FileConfilictPopup();

	if (m_CurrentDirectory != m_LastDirectory)
	{
		m_NeedToExpandTree = true;
		m_LastDirectory = m_CurrentDirectory;
	}
	RefreshCacheIfNeeded();
	static ImGuiTableFlags tableFlags = ImGuiTableFlags_Resizable | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV;
	if (ImGui::BeginTable("ContentBrowserTableLayout", 2, tableFlags))
	{
		// 다국어 변경 시에도 폴더 트리 너비가 보존되도록 ##Folders, ##Files 추가
		const std::string colFolders = LOCAL("ContentBrowser.Folders") + "##Folders";
		const std::string colFiles = LOCAL("ContentBrowser.Files") + "##Files";
		ImGui::TableSetupColumn(colFolders.c_str(), ImGuiTableColumnFlags_WidthFixed, 250.0f);
		ImGui::TableSetupColumn(colFiles.c_str(), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		ImGui::BeginChild("TreeArea", ImVec2(0, 0), false);
		DrawDirectoryTree(m_RootPath, m_RootPath);
		std::error_code treeEc;
		if (m_ShowEngineContent && !m_EngineRootPath.empty() && m_EngineRootPath != m_RootPath &&
			std::filesystem::is_directory(m_EngineRootPath, treeEc))
		{
			ImGui::Separator();
			DrawDirectoryTree(m_EngineRootPath, m_EngineRootPath);
		}
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
	if (event->type == SDL_EVENT_DROP_FILE)
	{
		const char* droppedFile = event->drop.data;
		if (droppedFile)
		{
			vector<string> droppedFiles = { droppedFile };
			OnExteranalDropped(droppedFiles);
		}
	}
}
void ContentBrowserPanel::Free()
{
	// 감시 스레드가 종료될 때까지 리스너와 패널 상태를 유지합니다.
	Safe_Delete(m_FileWatcher);
	m_WatchID = 0;
	m_EngineWatchID = 0;
	Safe_Delete(m_FileListener);

	if (m_AsyncCompletionHandle != 0 && AssetManager::IsCreated())
		AssetManager::Get().OnAsyncCompleted().Remove(m_AsyncCompletionHandle);

	m_AsyncCompletionHandle = 0;
	m_ImportPreviewWidget.Free();
}

bool ContentBrowserPanel::CanRenameTarget() const
{
	if (!m_Open || m_RenameTargetPath.empty() || !m_RenamingPath.empty())
		return false;

	if (Engine::PlatformPath::IsReadOnlyTarget(m_RenameTargetPath))
		return false;

	const auto& activeRoot = GetActiveRootForPath(m_RenameTargetPath);
	const auto relative = m_RenameTargetPath.lexically_normal().lexically_relative(activeRoot.lexically_normal());
	if (relative.empty() || relative == "." || relative.is_absolute() || *relative.begin() == "..")
		return false;

	std::error_code error;
	if (!filesystem::exists(m_RenameTargetPath, error) || error)
		return false;

	const auto filename = m_RenameTargetPath.filename().u8string();
	return !filename.empty() && filename.size() < sizeof(m_RenameBuffer);
}

void ContentBrowserPanel::RenameTarget()
{
	if (!CanRenameTarget())
		return;

	const auto filename = m_RenameTargetPath.filename().u8string();
	m_RenamingPath = m_RenameTargetPath;
	std::memcpy(m_RenameBuffer, filename.data(), filename.size());
	m_RenameBuffer[filename.size()] = '\0';
}

#pragma region TreeView
void ContentBrowserPanel::DrawDirectoryTree(const filesystem::path& path, const filesystem::path& rootPath)
{
	auto it = m_CachedSubdirectories.find(path);
	if (it == m_CachedSubdirectories.end())
	{
		vector<filesystem::path> subDirs;
		std::error_code ec;
		for (const auto& directoryEntry : filesystem::directory_iterator(path, ec))
		{
			if (!ec && directoryEntry.is_directory(ec))
			{
				if (!m_ShowEngineContent && Engine::PlatformPath::IsReadOnlyTarget(directoryEntry.path()))
					continue;
				subDirs.push_back(directoryEntry.path());
			}
		}
		it = m_CachedSubdirectories.emplace(path, std::move(subDirs)).first;
	}
	const vector<filesystem::path>& childDirectories = it->second;
	const bool hasChildDirectory = !childDirectories.empty();
	const bool isSelected = (m_CurrentDirectory == path);
	bool shouldDrawChildren = ContentBrowserTreeItem::DrawNode(
		path,
		rootPath,
		hasChildDirectory,
		isSelected,
		m_NeedToExpandTree,
		m_RenamingPath,
		m_RenameBuffer,
		sizeof(m_RenameBuffer),
		m_CurrentDirectory
	);
	if (shouldDrawChildren)
	{
		for (const auto& childPath : childDirectories)
			DrawDirectoryTree(childPath, rootPath);
		ImGui::TreePop();
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

	if (ImGui::BeginChild("GridArea", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
	{
		if (ImGui::IsWindowHovered() && InputManager::Get().IsCtrlPressed())
		{
			float wheel = InputManager::Get().GetMouseScrollDeltaY();
			if (wheel != 0.0f)
			{
				m_ThumbnailSize += wheel * 10.0f; // 스크롤 민감도

				// 크기 한계 지정
				if (m_ThumbnailSize < 32.0f) m_ThumbnailSize = 32.0f;
				if (m_ThumbnailSize > 256.0f) m_ThumbnailSize = 256.0f;
			}
		}

		DrawGrid(enterPressed);
		GridViewContextMenu(m_CurrentDirectory);
	}
	ImGui::EndChild();
}

void ContentBrowserPanel::GridViewContextMenu(const filesystem::path& path)
{
	if (ImGui::BeginPopupContextWindow("GridContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		const bool isReadOnly = Engine::PlatformPath::IsReadOnlyTarget(path);
		ImGui::BeginDisabled(isReadOnly);
		if (ImGui::BeginMenu((ICON_FA_PLUS " " + LOCAL_LABEL("EditorCommon.Create")).c_str()))
		{
			if (ImGui::MenuItem((ICON_FA_FOLDER " " + LOCAL_LABEL("EditorCommon.NewFolder")).c_str()))
			{
				filesystem::path newFolderPath = m_CurrentDirectory / "NewFolder";
				int suffix = 1;
				while (filesystem::exists(newFolderPath))
				{
					newFolderPath = m_CurrentDirectory / ("NewFolder_" + std::to_string(suffix++));
				}
				filesystem::create_directory(newFolderPath);
			}

			if (ImGui::MenuItem((ICON_FA_GLOBE " " + LOCAL_LABEL("ContentBrowser.CreateEmptyMaterial")).c_str()))
			{
				CreateEmptyMaterial(m_CurrentDirectory);
			}

			ImGui::EndMenu();
		}
		ImGui::EndDisabled();
		

		ImGui::Separator();

		if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.ShowInExplorer")).c_str()))
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
	const auto& activeRoot = GetActiveRootForPath(m_CurrentDirectory);
	bool isRoot = (m_CurrentDirectory == activeRoot);

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

	const auto& activeRoot = GetActiveRootForPath(m_CurrentDirectory);
	const bool isEngineRoot = (!m_EngineRootPath.empty() && m_EngineRootPath != m_RootPath && activeRoot == m_EngineRootPath);

	// 현재 경로를 분해하여 노드 리스트 생성
	std::vector<std::filesystem::path> nodes;
	std::filesystem::path current = m_CurrentDirectory;
	while (current != activeRoot && !current.empty() && current != current.parent_path())
	{
		nodes.push_back(current);
		current = current.parent_path();
	}
	nodes.push_back(activeRoot);
	std::reverse(nodes.begin(), nodes.end()); // Root부터 시작하도록 순서 뒤집기

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		std::string name = (i == 0)
			? (isEngineRoot ? LOCAL("ContentBrowser.EngineContent") : LOCAL("ContentBrowser.Assets"))
			: nodes[i].filename().string();
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
	ImGui::SetNextItemWidth(130.f);
	bool enter = ImGui::InputTextWithHint("##SearchBar",
		(ICON_FA_MAGNIFYING_GLASS " " + LOCAL("EditorCommon.Search")).c_str(), m_SearchBuffer, sizeof(m_SearchBuffer),
		ImGuiInputTextFlags_EnterReturnsTrue);

	ImGui::SameLine();
	const char* scopeKey = m_SearchAllFolders ? "ContentBrowser.ScopeAll" : "ContentBrowser.ScopeCurrent";
	const string scopeLabel = LOCAL(scopeKey) + "###ContentSearchScope";
	if (ImGui::Button(scopeLabel.c_str()))
	{
		m_SearchAllFolders = !m_SearchAllFolders;
		enter = true; // 검색 범위 전환 시 즉시 재검색 실행
	}
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", LOCAL_CSTR(m_SearchAllFolders ?
			"ContentBrowser.ScopeAllHint" : "ContentBrowser.ScopeCurrentHint"));

	return enter;
}

void ContentBrowserPanel::ContentFilter()
{
	const char* filterKeys[] = { "ResourceType.Texture", "ResourceType.Model", "ResourceType.Material",
		"ResourceType.Shader", "ContentBrowser.Data" };
	int filterCount = IM_ARRAYSIZE(filterKeys);

	// 콤보박스 타이틀 설정 로직
	std::string previewText = LOCAL("EditorCommon.All");
	if (m_FilterMask != 0)
	{
		int activeCount = 0;
		std::string activeNames = "";
		for (int i = 0; i < filterCount; ++i)
		{
			if (m_FilterMask & (1 << i))
			{
				if (activeCount > 0) activeNames += ", ";
				activeNames += LOCAL(filterKeys[i]);
				activeCount++;
			}
		}

		if (activeCount == 1)
			previewText = activeNames;
		else if (activeCount > 1)
			previewText = LOCAL_FMT("ContentBrowser.Multiple", activeCount);
	}

	ImGui::SetNextItemWidth(135.f);
	if (ImGui::BeginCombo("##Filter", previewText.c_str()))
	{
		// "All" 항목 (마스크 초기화)
		bool allSelected = (m_FilterMask == 0);
		if (ImGui::Selectable(LOCAL_LABEL("EditorCommon.All").c_str(), allSelected))
		{
			m_FilterMask = 0;
		}
		ImGui::Separator();

		// 각 필터별 체크박스
		for (int i = 0; i < filterCount; ++i)
		{
			bool isSelected = (m_FilterMask & (1 << i)) != 0;
			if (ImGui::Checkbox(LOCAL_LABEL(filterKeys[i]).c_str(), &isSelected))
			{
				if (isSelected)
					m_FilterMask |= (1 << i);
				else
					m_FilterMask &= ~(1 << i);
			}
		}

		ImGui::Separator();
		bool showEngine = m_ShowEngineContent;
		if (ImGui::Checkbox((ICON_FA_GEARS " " + LOCAL_LABEL("ContentBrowser.ShowEngineContent")).c_str(), &showEngine))
		{
			SetShowEngineContent(showEngine);
		}
		ImGui::EndCombo();
	}
}
#pragma endregion

void ContentBrowserPanel::DrawGridItem(const filesystem::directory_entry& directoryEntry, ImGuiSelectionUserData itemId)
{
	const auto& path = directoryEntry.path();

	ImTextureID thumbnailTexID = 0;
	if (ImGui::IsRectVisible(ImVec2(m_ThumbnailSize, m_ThumbnailSize)) && !directoryEntry.is_directory())
	{
		thumbnailTexID = ImGuiManager::Get().GetImGuiTextureID(AssetCache::Get().GetThumbnail(path));
	}

	const auto& activeRoot = GetActiveRootForPath(path);
	const bool keepAlive = ContentBrowserGridItem::Draw(directoryEntry, activeRoot, thumbnailTexID,
		m_ThumbnailSize, itemId, m_SelectedGridIds.contains(itemId), m_RenamingPath, m_RenameBuffer,
		sizeof(m_RenameBuffer), m_CurrentDirectory, m_SearchBuffer);

	if (keepAlive)
		m_InteractionPaths.push_back(path);
}

void ContentBrowserPanel::SyncGridSelection(const vector<const filesystem::directory_entry*>& entries)
{
	const bool sameOrder = std::equal(entries.begin(), entries.end(), m_GridSelectionItems.begin(),
		m_GridSelectionItems.end(), [](const auto* entry, const GridSelectionItem& item)
		{
			return entry->path() == item.Path;
		});
	if (sameOrder)
		return;

	// 목록이 바뀌어도 남아 있는 경로의 선택 ID는 유지한다.
	std::unordered_map<filesystem::path, ImGuiSelectionUserData> previousIds;
	previousIds.reserve(m_GridSelectionItems.size());
	for (auto& item : m_GridSelectionItems)
		previousIds.emplace(std::move(item.Path), item.Id);

	m_GridSelectionItems.clear();
	m_GridSelectionItems.reserve(entries.size());
	m_GridSelectionIndices.clear();
	m_GridSelectionIndices.reserve(entries.size());
	for (const auto* entry : entries)
	{
		const auto& path = entry->path();
		const auto previous = previousIds.find(path);
		const auto id = previous != previousIds.end() ? previous->second : m_NextGridSelectionId++;
		m_GridSelectionIndices.emplace(id, static_cast<int>(m_GridSelectionItems.size()));
		m_GridSelectionItems.push_back({ path, id });
	}

	std::erase_if(m_SelectedGridIds, [this](ImGuiSelectionUserData id)
	{
		return !m_GridSelectionIndices.contains(id);
	});
}

void ContentBrowserPanel::ApplyGridSelectionRequests(const ImGuiMultiSelectIO& selection)
{
	for (const auto& request : selection.Requests)
	{
		if (request.Type == ImGuiSelectionRequestType_SetAll)
		{
			m_SelectedGridIds.clear();
			if (request.Selected)
			{
				for (const auto& item : m_GridSelectionItems)
					m_SelectedGridIds.insert(item.Id);
			}
			continue;
		}

		if (request.Type != ImGuiSelectionRequestType_SetRange)
			continue;

		const auto first = m_GridSelectionIndices.find(request.RangeFirstItem);
		const auto last = m_GridSelectionIndices.find(request.RangeLastItem);
		if (first == m_GridSelectionIndices.end() && last == m_GridSelectionIndices.end())
			continue;

		// Shift 기준이 사라진 프레임에는 남아 있는 끝점만 처리한다.
		const int firstIndex = first != m_GridSelectionIndices.end() ? first->second : last->second;
		const int lastIndex = last != m_GridSelectionIndices.end() ? last->second : firstIndex;
		const auto [begin, end] = std::minmax(firstIndex, lastIndex);
		for (int index = begin; index <= end; ++index)
		{
			const auto id = m_GridSelectionItems[index].Id;
			if (request.Selected)
				m_SelectedGridIds.insert(id);
			else
				m_SelectedGridIds.erase(id);
		}
	}
}

void ContentBrowserPanel::DrawGrid(bool enterPressed)
{
	std::vector<filesystem::path> interactionPaths;
	interactionPaths.swap(m_InteractionPaths);
	const string searchStr(m_SearchBuffer);
	const bool isSearching = !searchStr.empty();
	const bool searchCleared = !isSearching && !m_LastSearchString.empty();

	if (enterPressed)
	{
		m_LastSearchString = searchStr;
		m_SearchResults.clear();

		if (isSearching)
		{
			if (m_SearchAllFolders)
			{
				// 전체 리소스 폴더 재귀 검색
				std::error_code searchEc;
				for (const auto& entry : filesystem::recursive_directory_iterator(
					m_RootPath, filesystem::directory_options::skip_permission_denied, searchEc))
				{
					if (!m_ShowEngineContent && Engine::PlatformPath::IsReadOnlyTarget(entry.path()))
						continue;
					if (StringContainsCaseInsensitive(entry.path().filename().string(), searchStr))
						m_SearchResults.push_back(entry);
				}

				if (m_ShowEngineContent && !m_EngineRootPath.empty() && m_EngineRootPath != m_RootPath &&
					std::filesystem::is_directory(m_EngineRootPath, searchEc))
				{
					for (const auto& entry : filesystem::recursive_directory_iterator(
						m_EngineRootPath, filesystem::directory_options::skip_permission_denied, searchEc))
					{
						if (StringContainsCaseInsensitive(entry.path().filename().string(), searchStr))
							m_SearchResults.push_back(entry);
					}
				}
			}
			else
			{
				// 현재 디렉토리 내부만 검색
				for (const auto& entry : filesystem::directory_iterator(
					m_CurrentDirectory, filesystem::directory_options::skip_permission_denied))
				{
					if (!m_ShowEngineContent && Engine::PlatformPath::IsReadOnlyTarget(entry.path()))
						continue;
					if (StringContainsCaseInsensitive(entry.path().filename().string(), searchStr))
						m_SearchResults.push_back(entry);
				}
			}
		}
	}

	if (!isSearching)
	{
		m_LastSearchString.clear();
		m_SearchResults.clear();
	}

	if (enterPressed || searchCleared)
		ImGui::SetScrollY(0.0f);

	// 파일 항목을 복사하지 않고 이번 프레임에 사용할 참조 목록을 만듭니다.
	const auto& source = isSearching ? m_SearchResults : m_CachedEntries;
	std::vector<const filesystem::directory_entry*> entries;
	entries.reserve(source.size());

	for (const auto& entry : source)
	{
		if (!m_ShowEngineContent && Engine::PlatformPath::IsReadOnlyTarget(entry.path()))
			continue;
		if (PassesFilter(entry, m_FilterMask))
			entries.push_back(&entry);
	}

	SyncGridSelection(entries);

	int revealIndex = -1;
	if (m_RevealScrollPending)
	{
		for (int index = 0; index < static_cast<int>(entries.size()); ++index)
		{
			if (entries[index]->path().lexically_normal() == m_RevealedAssetPath)
			{
				revealIndex = index;
				break;
			}
		}

		if (revealIndex < 0)
			m_RevealScrollPending = false;
	}

	const int itemCount = static_cast<int>(entries.size());
	// 타일은 마우스를 뗄 때 선택하여 타일에서 시작하는 기존 드롭과 구분한다.
	const ImGuiMultiSelectFlags selectionFlags = ImGuiMultiSelectFlags_ScopeWindow | ImGuiMultiSelectFlags_BoxSelect2d |
		ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_SelectOnClickRelease |
		ImGuiMultiSelectFlags_NoSelectAll | ImGuiMultiSelectFlags_NoSelectOnRightClick;
	const auto* selection = ImGui::BeginMultiSelect(selectionFlags, static_cast<int>(m_SelectedGridIds.size()), itemCount);
	ApplyGridSelectionRequests(*selection);
	const auto rangeSource = m_GridSelectionIndices.find(selection->RangeSrcItem);

	if (entries.empty())
	{
		if (isSearching)
		{
			ImGui::TextDisabled("%s: \"%s\"", LOCAL_CSTR("ContentBrowser.NoSearchResults"), searchStr.c_str());
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("ContentBrowser.ClearSearch")))
			{
				m_SearchBuffer[0] = '\0';
				m_SearchResults.clear();
				m_LastSearchString.clear();
			}
		}
		else
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("ContentBrowser.EmptyFolder"));
		}
		ApplyGridSelectionRequests(*ImGui::EndMultiSelect());
		return;
	}

	const float cellSize = m_ThumbnailSize + m_Padding;
	const float panelWidth = ImGui::GetContentRegionAvail().x;
	const int columnCount = std::clamp(static_cast<int>(panelWidth / cellSize), 1, 64);
	const int rowCount = (itemCount + columnCount - 1) / columnCount;

	ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(m_Padding * 0.5f, m_Padding * 0.5f));

	const ImGuiTableFlags flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoSavedSettings;
	if (!ImGui::BeginTable("ContentBrowserTable", columnCount, flags))
	{
		ImGui::PopStyleVar();
		ApplyGridSelectionRequests(*ImGui::EndMultiSelect());
		return;
	}

	for (int column = 0; column < columnCount; ++column)
		ImGui::TableSetupColumn("##AssetColumn", ImGuiTableColumnFlags_WidthFixed, m_ThumbnailSize);

	// 이름 변경 입력창도 같은 행 높이 안에 들어가도록 최소 높이를 확보합니다.
	const float nameHeight = ContentBrowserGridItem::GetNameBoxHeight();
	const float rowHeight = std::ceil(m_ThumbnailSize + nameHeight + ImGui::GetStyle().CellPadding.y * 2.0f);

	ImGuiListClipper clipper;
	clipper.Begin(rowCount, rowHeight);
	if (rangeSource != m_GridSelectionIndices.end())
		clipper.IncludeItemByIndex(rangeSource->second / columnCount);
	if (revealIndex >= 0)
		clipper.IncludeItemByIndex(revealIndex / columnCount);

	if (!m_RenamingPath.empty() || !interactionPaths.empty())
	{
		for (int index = 0; index < itemCount; ++index)
		{
			const auto& path = entries[index]->path();
			const bool interacting = std::find(interactionPaths.begin(), interactionPaths.end(), path)
				!= interactionPaths.end();

			if (path == m_RenamingPath || interacting)
				clipper.IncludeItemByIndex(index / columnCount);
		}
	}

	while (clipper.Step())
	{
		for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
		{
			ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

			for (int column = 0; column < columnCount; ++column)
			{
				const int index = row * columnCount + column;
				if (index >= itemCount)
					break;

				ImGui::TableSetColumnIndex(column);
				const ImVec2 tileMin = ImGui::GetCursorScreenPos();
				DrawGridItem(*entries[index], m_GridSelectionItems[index].Id);

				if (!m_RevealedAssetPath.empty() && entries[index]->path().lexically_normal() == m_RevealedAssetPath)
				{
					const ImVec2 tileMax(tileMin.x + m_ThumbnailSize, tileMin.y + m_ThumbnailSize);
					ImGui::GetWindowDrawList()->AddRect(tileMin, tileMax, ImGui::GetColorU32(ImGuiCol_CheckMark), 4.0f);

					if (index == revealIndex)
					{
						ImGui::SetScrollHereY(0.5f);
						m_RevealScrollPending = false;
					}
				}
			}
		}
	}

	ImGui::EndTable();
	ImGui::PopStyleVar();
	ApplyGridSelectionRequests(*ImGui::EndMultiSelect());
}

void ContentBrowserPanel::FileConfilictPopup()
{
	LocalizationManager& localizationManager = LocalizationManager::Get();
	const string popupTitle = LOCAL_LABEL("ContentBrowser.FileConflictPopup");
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
bool ContentBrowserPanel::RequestRevealAsset(const filesystem::path& resourcePath)
{
	if (resourcePath.empty())
		return false;

	auto absolutePath = Engine::PlatformPath::ResolveUri(resourcePath);
	std::error_code error;
	if (!filesystem::is_regular_file(absolutePath, error))
	{
		const auto resolved = ImGuiManager::Get().ResolveResourceEditorTarget(resourcePath);
		if (!resolved.empty() && filesystem::is_regular_file(resolved, error))
			absolutePath = resolved;
		else
			return false;
	}

	if (Engine::PlatformPath::IsReadOnlyTarget(absolutePath) && !m_ShowEngineContent)
		SetShowEngineContent(true);

	const auto& activeRoot = GetActiveRootForPath(absolutePath);
	const auto relativePath = filesystem::relative(absolutePath, activeRoot, error);
	if (error || relativePath.empty() || relativePath.has_root_path() || *relativePath.begin() == L"..")
		return false;

	m_RevealedAssetPath = (activeRoot / relativePath).lexically_normal();
	m_CurrentDirectory = m_RevealedAssetPath.parent_path();
	m_RevealScrollPending = true;
	m_NeedToExpandTree = true;

	m_SearchBuffer[0] = '\0';
	m_LastSearchString.clear();
	m_SearchResults.clear();
	m_FilterMask = 0;

	RequestRefresh();
	Focus();
	return true;
}
void ContentBrowserPanel::RefreshCacheIfNeeded()
{
	const bool fileSystemChanged = m_NeedsCacheRefresh.exchange(false);
	const bool directoryChanged = (m_LastWatchedDirectory != m_CurrentDirectory);

	if (fileSystemChanged)
	{
		m_CachedSubdirectories.clear();
	}

	if (!fileSystemChanged && !directoryChanged)
	{
		return;
	}

	m_LastWatchedDirectory = m_CurrentDirectory;
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
		m_LastWatchedDirectory.clear();
		m_CachedSubdirectories.clear();
	}
}
void ContentBrowserPanel::OnExteranalDropped(const vector<string>& droppedFiles)
{
	if (Engine::PlatformPath::IsReadOnlyTarget(m_CurrentDirectory))
	{
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.ReadOnlyProtected"), ELogLevel::Warn, 3.0f);
		return;
	}

	for (const string& filepathStr : droppedFiles)
	{
		filesystem::path sourcePath(reinterpret_cast<const char8_t*>(filepathStr.c_str()));

		if (filesystem::exists(sourcePath))
		{
			std::string extension = sourcePath.extension().string();
			for (char& character : extension)
				character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));

			const bool isModel = extension == ".fbx" || extension == ".obj" || extension == ".gltf" || extension == ".glb";
			if (isModel)
			{
				if (auto* modal = ModelImportModal::Get())
					modal->Open(sourcePath, m_CurrentDirectory);
				continue;
			}
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
	const filesystem::path outputPath = path / L"NewMaterial.bammat.json";
	Material* material = Engine::Material::CreateEmpty();

	if (!material)
		return;

	auto& resourceManager = ResourceManager::Get();
	const EResult result = resourceManager.SaveToJsonFile(material, outputPath.wstring());
	resourceManager.DestroyResource(material);

	if (result != EResult::Success)
		EDITOR_LOG_WARN("Material saving is unavailable: {}", ResultToString(result));
}
#pragma endregion

#pragma region Workspace Settings
void ContentBrowserPanel::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);

	workspace.ContentBrowserDirectory = GetRelativeDirectory();
	workspace.ContentBrowserThumbnailSize = GetThumbnailSize();
	workspace.SetSplitter("ContentBrowser.ShowEngineContent", m_ShowEngineContent ? 1.0f : 0.0f);
}

void ContentBrowserPanel::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_ShowEngineContent = workspace.GetSplitter("ContentBrowser.ShowEngineContent", 0.0f) > 0.5f;
	RestoreViewState(workspace.ContentBrowserDirectory, workspace.ContentBrowserThumbnailSize);
}
#pragma endregion


void ContentBrowserPanel::RestoreViewState(const std::string& relativeDirectory, float thumbnailSize)
{
	m_ThumbnailSize = std::isfinite(thumbnailSize) && thumbnailSize >= 32.0f && thumbnailSize <= 256.0f
		? thumbnailSize : 64.0f;

	m_CurrentDirectory = m_RootPath;
	m_LastDirectory.clear();
	m_LastWatchedDirectory.clear();
	m_NeedToExpandTree = true;
	RequestRefresh();

	m_SearchBuffer[0] = '\0';
	m_LastSearchString.clear();
	m_SearchResults.clear();

	if (relativeDirectory.empty())
		return;

	try
	{
		const auto relative = std::filesystem::u8path(relativeDirectory).lexically_normal();

		if (relative.has_root_path() || (!relative.empty() && *relative.begin() == ".."))
		{
			EDITOR_LOG_WARN("Content browser directory rejected: {}", relativeDirectory);
			return;
		}

		const auto directory = (m_RootPath / relative).lexically_normal();
		std::error_code error;

		if (!std::filesystem::is_directory(directory, error))
		{
			EDITOR_LOG_WARN("Content browser directory unavailable: path={}, filesystem={}",
				relativeDirectory, error.message());
			return;
		}

		m_CurrentDirectory = directory;
	}
	catch (const std::filesystem::filesystem_error& error)
	{
		EDITOR_LOG_WARN("Content browser directory restore failed: {}", error.what());
	}
}

std::string ContentBrowserPanel::GetRelativeDirectory() const
{
	try
	{
		const auto relative = m_CurrentDirectory.lexically_relative(m_RootPath);

		if (relative.empty() || relative.has_root_path() || *relative.begin() == "..")
			return {};

		const auto utf8 = relative.generic_u8string();
		return std::string(utf8.begin(), utf8.end());
	}
	catch (const std::filesystem::filesystem_error& error)
	{
		EDITOR_LOG_WARN("Content browser directory capture failed: {}", error.what());
		return {};
	}
}

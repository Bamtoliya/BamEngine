#pragma once
#include "InputManager.h"
#include "EditorUI.h"
#include "ContentBrowserPanel.h"
#include "PickerAssetCatalog.h"
#include "AssetPathRelocation.h"
#include "AssetReferencePanel.h"
#include "ResourceManager.h"
#include "ActionTimelineAsset.h"
#include "ContentBrowserGridItem.h"
#include "ContentBrowserTreeItem.h"
#include "ContentBrowserDragPayload.h"
#include "ContentBrowserRename.h"
#include "PIEManager.h"
#include "Editor_Macros.h"
#include "EditorSettings.h"
#include <stb_image.h>
#include "LocalizationManager.h"
#include "AssetManager.h"
#include "ImGuiManager.h"
#include "EditorSettingsService.h"
#include "ShortcutManager.h"
#include "MaterialInstance.h"
#include "ModelImportModal.h"
#include "NotificationManager.h"
#include "ProjectSessionManager.h"
#include "FileDialogs.h"
#include "SpriteSheetBaker.h"
#include "Model.h"
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include <algorithm>
#include <chrono>
#include <string_view>
#include <utility>

using namespace std;

static bool ContentBrowserPathWithin(const filesystem::path& path, const filesystem::path& root)
{
	if (root.empty())
		return false;

	const auto sameComponent = [](const auto& lhs, const auto& rhs)
		{
			return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_EQUAL;
		};
	return std::mismatch(path.begin(), path.end(), root.begin(), root.end(), sameComponent).second == root.end();
}

#pragma region ContentBrowserFileListner
void ContentBrowserFileListener::QueueReferenceChanges(AssetReferenceChanges changes)
{
	if (changes.Empty()) return;
	std::lock_guard lock(m_ReferenceChangesMutex);
	m_ReferenceChanges.Merge(std::move(changes));
	m_HasReferenceChanges.store(true);
}

AssetReferenceChanges ContentBrowserFileListener::TakeReferenceChanges()
{
	std::lock_guard lock(m_ReferenceChangesMutex);
	m_HasReferenceChanges.store(false);
	return std::exchange(m_ReferenceChanges, {});
}

void ContentBrowserFileListener::handleFileAction(efsw::WatchID watchid, const std::string& dir,
	const std::string& filename, efsw::Action action, std::string oldFilename)
{
	if (action != efsw::Action::Modified && action != efsw::Action::Add &&
		action != efsw::Action::Delete && action != efsw::Action::Moved)
	{
		return;
	}

	if (action != efsw::Action::Modified)
		Editor::Detail::InvalidatePickerAssetCatalog();

	try
	{
		// C++20 표준 방식: reinterpret_cast<const char8_t*>를 사용해 UTF-8 문자열임을 명시하여 path 생성
		const filesystem::path directory(reinterpret_cast<const char8_t*>(dir.c_str()));
		auto& cache = AssetCache::Get();

		const filesystem::path filePath = directory / reinterpret_cast<const char8_t*>(filename.c_str());
		const bool projectWatch = watchid == m_ProjectWatchId;
		AssetReferenceChanges changes;
		changes.ObserveFile(filePath, projectWatch, action != efsw::Action::Modified);
		cache.RequestInvalidate(filePath);

		if (action == efsw::Action::Moved)
		{
			if (oldFilename.empty())
				changes.RequireFullScan();
			else
			{
				const filesystem::path oldFilePath = directory / reinterpret_cast<const char8_t*>(oldFilename.c_str());
				changes.ObserveFile(oldFilePath, projectWatch, true);
				cache.RequestInvalidate(oldFilePath);
			}
		}

		QueueReferenceChanges(std::move(changes));
		m_RefreshFlag.store(true);
	}
	catch (const std::exception&)
	{
		QueueReferenceChanges({ true, {} });
		m_RefreshFlag.store(true);
	}
}
#pragma endregion

static bool StringContainsCaseInsensitive(std::wstring_view filename, std::wstring_view query)
{
	if (query.empty()) return true;

	const auto length = static_cast<int>(query.size());
	for (size_t offset = 0; offset + query.size() <= filename.size(); ++offset)
	{
		if (CompareStringOrdinal(filename.data() + offset, length, query.data(), length, TRUE) == CSTR_EQUAL)
			return true;
	}
	return false;
}

static bool PassesFilter(const filesystem::directory_entry& entry, uint32 filterMask)
{
	if (filterMask == 0) return true; // 아무것도 선택하지 않거나 "All"인 경우 모두 표시
	if (entry.is_directory()) return true; // 폴더는 무조건 표시
	const auto filename = Engine::ToLowerStr(Engine::WStrToStr(entry.path().filename().wstring()));
	const auto ext = Engine::ToLowerStr(Engine::WStrToStr(entry.path().extension().wstring()));
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
	m_EditorRootPath = Engine::PlatformPath::GetVirtualSchemeRoot(L"Editor");
	m_ShowEngineContent = true;
	m_CurrentDirectory = m_RootPath;
	m_LastDirectory = m_CurrentDirectory;
	m_NeedToExpandTree = true;
	memset(m_SearchBuffer, 0, sizeof(m_SearchBuffer));
	m_FileWatcher = new efsw::FileWatcher();
	m_FileListener = new ContentBrowserFileListener(m_NeedsCacheRefresh);
	m_WatchID = m_FileWatcher->addWatch(m_RootPath.string(), m_FileListener, true);
	m_FileListener->SetProjectWatchId(m_WatchID);
	std::error_code ec;
	if (m_EngineRootPath != m_RootPath && std::filesystem::is_directory(m_EngineRootPath, ec))
		m_EngineWatchID = m_FileWatcher->addWatch(m_EngineRootPath.string(), m_FileListener, true);
	if (!m_EditorRootPath.empty() && m_EditorRootPath != m_RootPath && m_EditorRootPath != m_EngineRootPath && std::filesystem::is_directory(m_EditorRootPath, ec))
		m_EditorWatchID = m_FileWatcher->addWatch(m_EditorRootPath.string(), m_FileListener, true);
	m_FileWatcher->watch();
	m_AsyncCompletionHandle = AssetManager::Get().OnAsyncCompleted().AddLambda([this]()
		{
			RequestRefresh();
		});
	m_ImportPreviewWidget.Initialize(L"ImportModalPreview", 380, 380, true);
	return EResult::Success;
}

void ContentBrowserPanel::RebindProjectRoot(const filesystem::path& newAssetRoot)
{
	if (newAssetRoot.empty())
		return;
	m_RootPath = newAssetRoot;
	if (m_FileWatcher && m_FileListener)
	{
		m_FileWatcher->removeWatch(m_WatchID);
		m_WatchID = m_FileWatcher->addWatch(m_RootPath.string(), m_FileListener, true);
		m_FileListener->SetProjectWatchId(m_WatchID);
	}
	m_CachedSubdirectories.clear();
	m_ReadOnlyCache.clear();
	m_CurrentDirectory = m_RootPath;
	m_LastDirectory = m_CurrentDirectory;
	m_NeedToExpandTree = true;
	RequestRefresh();
}

void ContentBrowserPanel::SetShowEngineContent(bool show)
{
	if (m_ShowEngineContent == show)
		return;
	m_ShowEngineContent = show;
	m_CachedSubdirectories.clear();
	m_ReadOnlyCache.clear();
	if (!m_ShowEngineContent && Engine::PlatformPath::IsReadOnlyTarget(m_CurrentDirectory))
	{
		m_CurrentDirectory = m_RootPath;
		m_NeedToExpandTree = true;
	}
	RequestRefresh();
}

const filesystem::path& ContentBrowserPanel::GetActiveRootForPath(const filesystem::path& path) const
{
	const auto normPath = Engine::PlatformPath::Normalize(path);
	if (!m_EditorRootPath.empty() && m_EditorRootPath != m_RootPath)
	{
		const auto normEditor = Engine::PlatformPath::Normalize(m_EditorRootPath);
		if (normPath == normEditor)
			return m_EditorRootPath;
		const auto rel = normPath.lexically_relative(normEditor);
		if (!rel.empty() && !rel.is_absolute() && !rel.native().starts_with(L".."))
			return m_EditorRootPath;
	}
	if (!m_EngineRootPath.empty() && m_EngineRootPath != m_RootPath)
	{
		const auto normEngine = Engine::PlatformPath::Normalize(m_EngineRootPath);
		if (normPath == normEngine)
			return m_EngineRootPath;
		const auto rel = normPath.lexically_relative(normEngine);
		if (!rel.empty() && !rel.is_absolute() && !rel.native().starts_with(L".."))
			return m_EngineRootPath;
	}
	return m_RootPath;
}

bool ContentBrowserPanel::IsExternalUnregisteredDirectory(const filesystem::path& path) const
{
	if (path.empty())
		return false;

	const auto normPath = Engine::PlatformPath::Normalize(path);

	if (!m_RootPath.empty())
	{
		const auto normRoot = Engine::PlatformPath::Normalize(m_RootPath);
		if (normPath == normRoot)
			return false;
		const auto rel = normPath.lexically_relative(normRoot);
		if (!rel.empty() && !rel.is_absolute() && !rel.native().starts_with(L".."))
			return false;
	}

	if (!m_EngineRootPath.empty())
	{
		const auto normEngine = Engine::PlatformPath::Normalize(m_EngineRootPath);
		if (normPath == normEngine)
			return false;
		const auto rel = normPath.lexically_relative(normEngine);
		if (!rel.empty() && !rel.is_absolute() && !rel.native().starts_with(L".."))
			return false;
	}

	if (!m_EditorRootPath.empty())
	{
		const auto normEditor = Engine::PlatformPath::Normalize(m_EditorRootPath);
		if (normPath == normEditor)
			return false;
		const auto rel = normPath.lexically_relative(normEditor);
		if (!rel.empty() && !rel.is_absolute() && !rel.native().starts_with(L".."))
			return false;
	}

	const auto contentMounts = Engine::PlatformPath::GetContentMounts();
	for (const auto& mount : contentMounts)
	{
		if (mount.RootPath.empty())
			continue;
		const auto normMount = Engine::PlatformPath::Normalize(mount.RootPath);
		if (normPath == normMount)
			return false;
		const auto rel = normPath.lexically_relative(normMount);
		if (!rel.empty() && !rel.is_absolute() && !rel.native().starts_with(L".."))
			return false;
	}

	return true;
}

static void RequestAssetReferenceScanRefresh(AssetReferenceChanges changes = { true, {} })
{
	auto* panel = ImGuiManager::Get().GetImGuiPanel(L"AssetReferenceChecker");
	if (auto* references = dynamic_cast<AssetReferencePanel*>(panel))
		references->RequestScanRefresh(std::move(changes));
}

static void PrimeAssetReferencePathRoots()
{
	static_cast<void>(Engine::PlatformPath::GetProjectRoot());
	static_cast<void>(Engine::PlatformPath::GetProjectAssetRoot());
	static_cast<void>(Engine::PlatformPath::GetEngineResourceRoot());
	static_cast<void>(Engine::PlatformPath::GetVirtualSchemeRoot(L"Editor"));
}

void ContentBrowserPanel::Update(f32 dt)
{
	ProcessMovePreparation();
	ProcessDeletionPreparation();

	if (m_FileListener && m_FileListener->HasReferenceChanges())
		RequestAssetReferenceScanRefresh(m_FileListener->TakeReferenceChanges());

	if (m_Open && m_IsImportModalActive)
	{
		m_ImportPreviewWidget.BeginFrame(dt);
		m_ImportPreviewWidget.Submit(dt);
	}
}

void ContentBrowserPanel::ProcessMovePreparation()
{
	if (!m_MovePreparationPending)
		return;
	if (!m_Open)
		m_MovePreparationCancelled = true;
	if (!m_MovePreparation.valid() ||
		m_MovePreparation.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return;

	m_MovePreparationPending = false;
	try
	{
		auto result = m_MovePreparation.get();
		AssetManager::Get().RecordBackgroundAssetReferenceTiming(
			AssetReferenceTimingKind::MovePreparation, result.ElapsedMs);
		if (m_MovePreparationCancelled)
		{
			m_MovePreparationCancelled = false;
			ClearPendingFileMove();
			return;
		}

		m_MoveImpact = std::move(result.Impact);
		m_MoveReferences = std::move(result.References);
		m_MoveReferencesReady = result.ReferencesReady;
		m_MoveDocumentsAvailable = m_MoveReferencesReady &&
			CanApplyAssetMove(m_PendingFileMove, m_MoveReferences, &m_MoveBlock);
		if (m_PendingRename && m_MoveReferencesReady && m_MoveDocumentsAvailable && m_MoveImpact.References.empty())
			ConfirmFileMove();
	}
	catch (const std::exception& error)
	{
		if (m_MovePreparationCancelled)
		{
			m_MovePreparationCancelled = false;
			ClearPendingFileMove();
			return;
		}

		EDITOR_LOG_WARN("Could not prepare asset move references: {}", error.what());
		m_MoveReferencesReady = false;
		m_MoveImpact.IsComplete = false;
		m_MoveDocumentsAvailable = false;
	}
}

void ContentBrowserPanel::DrawFolderDropTarget(const filesystem::path& directory)
{
	if (m_DragPaths.empty() || !ImGui::BeginDragDropTarget())
		return;

	if (CanTransferTo(directory))
	{
		const auto* payload = ImGui::AcceptDragDropPayload(ContentBrowser::MultipleItemsPayload);
		if (!payload)
			payload = ImGui::AcceptDragDropPayload(ContentBrowser::SingleItemPayload);

		if (payload && payload->IsDelivery())
			m_PendingFileDropDirectory = directory;
	}

	ImGui::EndDragDropTarget();
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
	ProcessQueuedRename();

	if (m_Hovered && m_RenamingPath.empty() && !ImGui::GetIO().WantTextInput &&
		!ImGui::IsAnyItemActive() && !ImGui::GetDragDropPayload() &&
		!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
	{
		auto& input = InputManager::Get();
		if (input.IsMouseButtonDown(EMouseButton::X1))
			StepFolderHistory(false);
		else if (input.IsMouseButtonDown(EMouseButton::X2))
			StepFolderHistory(true);
	}

	FileConfilictPopup();
	DrawFileDeletionConfirmation();
	DrawFileMoveConfirmation();
	DrawMountAddModal();
	DrawSpriteSheetBakeModal();

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
		std::error_code treeEc;

		// 1. Engine Resources (상위 배치)
		if (!m_EngineRootPath.empty() && m_EngineRootPath != m_RootPath &&
			std::filesystem::is_directory(m_EngineRootPath, treeEc))
		{
			DrawDirectoryTree(m_EngineRootPath, m_EngineRootPath);
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
		}

		// 2. Editor Resources (상위 배치)
		if (!m_EditorRootPath.empty() && m_EditorRootPath != m_RootPath && m_EditorRootPath != m_EngineRootPath &&
			std::filesystem::is_directory(m_EditorRootPath, treeEc))
		{
			DrawDirectoryTree(m_EditorRootPath, m_EditorRootPath);
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
		}

		// 3. Project Assets
		DrawDirectoryTree(m_RootPath, m_RootPath);

		// 4. External Mounts
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		DrawExternalMountsTree();
		ImGui::EndChild();
		ImGui::TableNextColumn();
		DrawDirectoryGrid();
		ImGui::EndTable();
	}
	if (ImGui::IsWindowAppearing()) m_NeedToExpandTree = true;
	ImGui::End();
	m_NeedToExpandTree = false;
	if (m_SubmittedRename)
	{
		const auto rename = std::move(*m_SubmittedRename);
		m_SubmittedRename.reset();
		QueueAssetPathChanges({ rename }, false, true);
	}

	if (!m_PendingFileDropDirectory.empty())
	{
		const auto directory = std::exchange(m_PendingFileDropDirectory, filesystem::path{});
		QueueFileMove(m_DragPaths, directory, false);
	}
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
	m_DeletionPreparationCancelled = true;
	if (m_DeletionPreparation.valid())
	{
		m_DeletionPreparation.wait();
		if (m_DeletionPreparationWorker.joinable())
			m_DeletionPreparationWorker.join();
		try
		{
			auto result = m_DeletionPreparation.get();
			if (result.Error)
				std::rethrow_exception(result.Error);
		}
		catch (const std::exception& error)
		{
			EDITOR_LOG_WARN("Could not finish deletion impact analysis during shutdown: {}", error.what());
		}
		catch (...)
		{
			EDITOR_LOG_WARN("Could not finish deletion impact analysis during shutdown: unknown worker exception");
		}
	}
	else if (m_DeletionPreparationWorker.joinable())
		m_DeletionPreparationWorker.join();
	m_DeletionPreparationPending = false;
	m_DeletionPreparationCancelled = false;
	m_PendingFileDeletion.clear();
	m_DeletionImpact = {};

	m_MovePreparationCancelled = true;
	if (m_MovePreparation.valid())
	{
		m_MovePreparation.wait();
		try
		{
			m_MovePreparation.get();
		}
		catch (const std::exception& error)
		{
			EDITOR_LOG_WARN("Could not finish asset move preparation during shutdown: {}", error.what());
		}
	}
	m_MovePreparationPending = false;
	m_MovePreparationCancelled = false;
	ClearPendingFileMove();

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
	if (!m_Open || !PIEManager::Get().IsEditing() || m_RenameTargetPath.empty() || !m_RenamingPath.empty() ||
		!m_PendingFileMove.empty() || !m_PendingFileDeletion.empty() || m_MovePreparationPending ||
		m_DeletionPreparationPending)
		return false;

	if (Engine::PlatformPath::IsReadOnlyTarget(m_RenameTargetPath) || IsExternalUnregisteredDirectory(m_RenameTargetPath))
		return false;

	const auto& activeRoot = GetActiveRootForPath(m_RenameTargetPath);
	const auto relative = m_RenameTargetPath.lexically_normal().lexically_relative(activeRoot.lexically_normal());
	if (relative.empty() || relative == "." || relative.is_absolute() || *relative.begin() == "..")
		return false;

	std::error_code error;
	if (!filesystem::exists(m_RenameTargetPath, error) || error)
		return false;

	const auto filename = ContentBrowser::GetRenameBaseName(m_RenameTargetPath,
		filesystem::is_directory(m_RenameTargetPath, error));
	return !error && !filename.empty() && filename.size() < sizeof(m_RenameBuffer);
}

void ContentBrowserPanel::RenameTarget()
{
	if (!CanRenameTarget())
		return;

	BeginRename(m_RenameTargetPath, m_RenameTargetInTree);
}

void ContentBrowserPanel::BeginRename(const filesystem::path& path, bool inTree)
{
	std::error_code error;
	const auto filename = ContentBrowser::GetRenameBaseName(path, filesystem::is_directory(path, error));
	if (error || filename.empty() || filename.size() >= sizeof(m_RenameBuffer))
		return;
	m_RenamingPath = path;
	m_RenameInTree = inTree;
	m_RenameFocusRequested = true;
	std::memcpy(m_RenameBuffer, filename.data(), filename.size());
	m_RenameBuffer[filename.size()] = '\0';
}

void ContentBrowserPanel::QueueCreatedAssetRename(const filesystem::path& path)
{
	m_QueuedRenamePath = path;
	RequestRefresh();
}

void ContentBrowserPanel::ProcessQueuedRename()
{
	if (m_QueuedRenamePath.empty() || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
		return;
	const auto path = std::exchange(m_QueuedRenamePath, filesystem::path{});
	if (RequestRevealAsset(path))
		BeginRename(path, false);
}

void ContentBrowserPanel::DrawRenameEditor(const char* id, const filesystem::path& path)
{
	const auto result = ContentBrowser::DrawRenameInput(id, m_RenameBuffer, sizeof(m_RenameBuffer), m_RenameFocusRequested);
	if (ImGui::IsItemHovered())
		ImGui::SetTooltip("%s", LOCAL_CSTR("ContentBrowser.RenameInputHint"));
	if (result == ContentBrowser::ERenameInputResult::Cancel)
	{
		m_RenamingPath.clear();
		return;
	}
	if (result != ContentBrowser::ERenameInputResult::Submit)
		return;
	std::error_code error;
	filesystem::path target;
	if (!ContentBrowser::TryBuildRenamePath(path, filesystem::is_directory(path, error), m_RenameBuffer, target) || error)
	{
		m_RenameFocusRequested = true;
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.RenameInvalidName"), ELogLevel::Warn);
		return;
	}
	const auto source = path;
	const bool sameSpelling = CompareStringOrdinal(source.c_str(), -1, target.c_str(), -1, TRUE) == CSTR_EQUAL;
	if (filesystem::exists(target, error) && !sameSpelling)
	{
		m_RenameFocusRequested = true;
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.RenameNameExists"), ELogLevel::Warn);
		return;
	}
	m_RenamingPath.clear();
	if (source != target)
		m_SubmittedRename = AssetPathMove{ source, target };
}

#pragma region TreeView
void ContentBrowserPanel::DrawDirectoryTree(const filesystem::path& path, const filesystem::path& rootPath)
{
	auto it = m_CachedSubdirectories.find(path);
	if (it == m_CachedSubdirectories.end())
	{
		vector<filesystem::path> subDirs;
		std::error_code ec;
		const bool isInsideEngineOrEditor = (rootPath == m_EngineRootPath || rootPath == m_EditorRootPath);
		for (const auto& directoryEntry : filesystem::directory_iterator(path, ec))
		{
			if (!ec && directoryEntry.is_directory(ec))
			{
				if (!m_ShowEngineContent && !isInsideEngineOrEditor && Engine::PlatformPath::IsReadOnlyTarget(directoryEntry.path()))
					continue;
				subDirs.push_back(directoryEntry.path());
			}
		}
		it = m_CachedSubdirectories.emplace(path, std::move(subDirs)).first;
	}
	const vector<filesystem::path>& childDirectories = it->second;
	const bool hasChildDirectory = !childDirectories.empty();
	const bool isSelected = (m_CurrentDirectory == path);
	auto readOnlyIt = m_ReadOnlyCache.find(path.native());
	if (readOnlyIt == m_ReadOnlyCache.end())
		readOnlyIt = m_ReadOnlyCache.emplace(path.native(), Engine::PlatformPath::IsReadOnlyTarget(path)).first;
	const bool isRoot = (path.native() == rootPath.native());
	const bool isEngineRoot = isRoot && rootPath.native() == m_EngineRootPath.native();
	const bool isEditorRoot = isRoot && rootPath.native() == m_EditorRootPath.native();
	bool shouldDrawChildren = ContentBrowserTreeItem::DrawNode(*this, path, rootPath, hasChildDirectory, isSelected, m_NeedToExpandTree,
		readOnlyIt->second, isEngineRoot, isEditorRoot);
	if (shouldDrawChildren)
	{
		for (const auto& childPath : childDirectories)
			DrawDirectoryTree(childPath, rootPath);

		if (isRoot && rootPath.native() == m_RootPath.native())
		{
			const auto contentMounts = Engine::PlatformPath::GetContentMounts();
			for (const auto& mount : contentMounts)
			{
				if (!mount.ShowUnderAssets || mount.RootPath.empty())
					continue;
				std::error_code mountEc;
				if (!filesystem::is_directory(mount.RootPath, mountEc))
					continue;

				const bool mountSelected = Engine::PlatformPath::IsSamePath(m_CurrentDirectory, mount.RootPath);
				ImGuiTreeNodeFlags linkFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
				if (mountSelected)
					linkFlags |= ImGuiTreeNodeFlags_Selected;

				const std::string linkLabel = std::string(ICON_FA_LINK " ") + mount.Name + " " + LOCAL("ContentBrowser.MountLinkBadge") + "##MountAssetLink_" + mount.Name;
				ImGui::TreeNodeEx(linkLabel.c_str(), linkFlags);
				if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
					NavigateTo(mount.RootPath);
			}
		}

		ImGui::TreePop();
	}
}

void ContentBrowserPanel::DrawExternalMountsTree()
{
	ImGui::TextDisabled("%s", LOCAL_CSTR("ContentBrowser.ExternalMounts"));
	if (ImGui::SmallButton(LOCAL_LABEL("ContentBrowser.MountExternalFolder").c_str()))
		OpenMountAddModal();

	const auto contentMounts = Engine::PlatformPath::GetContentMounts();
	for (const auto& mount : contentMounts)
	{
		ImGui::PushID(mount.Name.c_str());
		std::error_code ec;
		const bool exists = !mount.RootPath.empty() && filesystem::is_directory(mount.RootPath, ec);
		const std::string headerLabel = std::string(ICON_FA_LINK " ") + mount.Name +
			(mount.ReadOnly ? " [" + LOCAL("ContentBrowser.MountReadOnly") + "]" : "") + "##ExternalMountNode";

		const bool selected = exists && Engine::PlatformPath::IsSamePath(m_CurrentDirectory, mount.RootPath);
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!exists)
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (selected)
			flags |= ImGuiTreeNodeFlags_Selected;

		const bool opened = ImGui::TreeNodeEx(headerLabel.c_str(), flags);
		if (exists && ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			NavigateTo(mount.RootPath);

		if (ImGui::BeginPopupContextItem("ExternalMountContextMenu"))
		{
			if (!ProjectSessionManager::IsCreated())
				ProjectSessionManager::Create();

			bool readOnly = mount.ReadOnly;
			if (ImGui::MenuItem(LOCAL_LABEL("ContentBrowser.MountReadOnly").c_str(), nullptr, &readOnly))
			{
				ProjectSessionManager::Get().SetContentMountFlags(mount.Name, readOnly, mount.ShowUnderAssets);
				m_ReadOnlyCache.clear();
				m_CachedSubdirectories.clear();
				RequestRefresh();
			}

			bool showUnderAssets = mount.ShowUnderAssets;
			if (ImGui::MenuItem(LOCAL_LABEL("ContentBrowser.MountShowUnderAssets").c_str(), nullptr, &showUnderAssets))
			{
				ProjectSessionManager::Get().SetContentMountFlags(mount.Name, mount.ReadOnly, showUnderAssets);
				RequestRefresh();
			}

			ImGui::Separator();
			if (ImGui::MenuItem(LOCAL_LABEL("ContentBrowser.Unmount").c_str()))
			{
				const bool insideMount = exists && !m_CurrentDirectory.empty() &&
					!Engine::PlatformPath::Normalize(m_CurrentDirectory).lexically_relative(Engine::PlatformPath::Normalize(mount.RootPath)).native().starts_with(L"..");
				ProjectSessionManager::Get().RemoveContentMount(mount.Name);
				if (insideMount)
					NavigateTo(m_RootPath);
				m_ReadOnlyCache.clear();
				m_CachedSubdirectories.clear();
				RequestRefresh();
			}
			ImGui::EndPopup();
		}

		if (exists && opened)
		{
			std::error_code subEc;
			for (const auto& entry : filesystem::directory_iterator(mount.RootPath, subEc))
			{
				if (!subEc && entry.is_directory(subEc))
					DrawDirectoryTree(entry.path(), mount.RootPath);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
}

void ContentBrowserPanel::OpenMountAddModal(const filesystem::path& initialPath)
{
	m_OpenMountAddModal = true;
	memset(m_MountNameBuffer, 0, sizeof(m_MountNameBuffer));
	memset(m_MountPathBuffer, 0, sizeof(m_MountPathBuffer));
	m_MountReadOnly = true;
	m_MountShowUnderAssets = false;
	m_MountError.clear();

	if (!initialPath.empty())
	{
		const std::string utf8Path = Engine::WStrToStr(initialPath.lexically_normal().wstring());
		const size_t copyPathLen = std::min(utf8Path.size(), sizeof(m_MountPathBuffer) - 1);
		memcpy(m_MountPathBuffer, utf8Path.data(), copyPathLen);
		m_MountPathBuffer[copyPathLen] = '\0';

		const auto fn = initialPath.filename().wstring();
		const std::wstring folderName = fn.empty() ? initialPath.wstring() : fn;
		const std::string utf8Name = Engine::WStrToStr(folderName);
		const size_t copyNameLen = std::min(utf8Name.size(), sizeof(m_MountNameBuffer) - 1);
		memcpy(m_MountNameBuffer, utf8Name.data(), copyNameLen);
		m_MountNameBuffer[copyNameLen] = '\0';
	}
}

void ContentBrowserPanel::DrawMountAddModal()
{
	constexpr const char* kPopupId = "MountExternalFolderPopup";
	const std::string modalTitle = LOCAL("ContentBrowser.MountAddTitle") + "###" + kPopupId;
	if (m_OpenMountAddModal)
	{
		ImGui::OpenPopup(modalTitle.c_str());
		m_OpenMountAddModal = false;
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(540.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::SetNextWindowSizeConstraints(ImVec2(540.0f, 0.0f), ImVec2(540.0f, FLT_MAX));

	bool open = true;
	ImVec4 modalBg = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
	modalBg.w = 1.0f;
	ImGui::PushStyleColor(ImGuiCol_PopupBg, modalBg);
	if (!ImGui::BeginPopupModal(modalTitle.c_str(), &open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PopStyleColor();
		return;
	}

	if (EditorUI::BeginPropertyTable("MountAddTable"))
	{
		EditorUI::PropertyRow("ContentBrowser.MountName");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputText("##MountNameInput", m_MountNameBuffer, sizeof(m_MountNameBuffer));

		EditorUI::PropertyRow("ContentBrowser.MountFolder");
		const float browseWidth = ImGui::CalcTextSize("...").x + ImGui::GetStyle().FramePadding.x * 2.0f + 12.0f;
		ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - browseWidth - ImGui::GetStyle().ItemSpacing.x));
		ImGui::InputText("##MountPathInput", m_MountPathBuffer, sizeof(m_MountPathBuffer));
		ImGui::SameLine();
		if (ImGui::Button("...##BrowseMountFolder", ImVec2(browseWidth, 0.0f)))
		{
			std::wstring selectedFolder;
			if (FileDialogs::OpenFolderDialog(selectedFolder, Engine::StrToWStr(m_MountPathBuffer), "ContentBrowser.MountFolder") && !selectedFolder.empty())
			{
				const std::string utf8 = Engine::WStrToStr(selectedFolder);
				const size_t copyLen = std::min(utf8.size(), sizeof(m_MountPathBuffer) - 1);
				memcpy(m_MountPathBuffer, utf8.data(), copyLen);
				m_MountPathBuffer[copyLen] = '\0';
				if (m_MountNameBuffer[0] == '\0')
				{
					const std::string folderName = Engine::WStrToStr(filesystem::path(selectedFolder).filename().wstring());
					const size_t nameLen = std::min(folderName.size(), sizeof(m_MountNameBuffer) - 1);
					memcpy(m_MountNameBuffer, folderName.data(), nameLen);
					m_MountNameBuffer[nameLen] = '\0';
				}
			}
		}

		EditorUI::PropertyRow("ContentBrowser.MountReadOnly");
		ImGui::Checkbox("##MountReadOnlyCheckbox", &m_MountReadOnly);

		EditorUI::PropertyRow("ContentBrowser.MountShowUnderAssets");
		ImGui::Checkbox("##MountShowUnderAssetsCheckbox", &m_MountShowUnderAssets);

		EditorUI::EndPropertyTable();
	}

	if (!m_MountError.empty())
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", m_MountError.c_str());
	}

	ImGui::Spacing();
	ImGui::Separator();
	const EditorUI::DialogAction actions[] = {
		{ "ContentBrowser.Cancel", true, false },
		{ "ContentBrowser.MountAddConfirm", true, false }
	};
	const int clicked = EditorUI::DialogButtonBar("MountAddFooter", actions, 2);
	if (clicked == 0)
	{
		m_MountError.clear();
		ImGui::CloseCurrentPopup();
	}
	else if (clicked == 1)
	{
		const std::string mountName(m_MountNameBuffer);
		const std::string mountPathUtf8(m_MountPathBuffer);
		if (mountName.empty() || mountPathUtf8.empty())
		{
			m_MountError = LOCAL("ContentBrowser.MountInvalidInput");
		}
		else
		{
			if (!ProjectSessionManager::IsCreated())
				ProjectSessionManager::Create();
			std::string error;
			if (ProjectSessionManager::Get().AddContentMount(mountName, filesystem::path(Engine::StrToWStr(mountPathUtf8)),
				m_MountReadOnly, m_MountShowUnderAssets, &error))
			{
				m_MountError.clear();
				m_ReadOnlyCache.clear();
				m_CachedSubdirectories.clear();
				RequestRefresh();
				ImGui::CloseCurrentPopup();
			}
			else
			{
				m_MountError = !error.empty() ? error : LOCAL("ContentBrowser.MountFailed");
			}
		}
	}

	ImGui::EndPopup();
	ImGui::PopStyleColor();
}
#pragma endregion

namespace
{
	bool ResizeRGBA8Bilinear(const std::vector<uint8>& src, int32 srcW, int32 srcH, int32 dstW, int32 dstH, std::vector<uint8>& outDst)
	{
		if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
			return false;
		if (src.size() < static_cast<size_t>(srcW) * static_cast<size_t>(srcH) * 4)
			return false;

		outDst.resize(static_cast<size_t>(dstW) * static_cast<size_t>(dstH) * 4);
		const float scaleX = static_cast<float>(srcW) / static_cast<float>(dstW);
		const float scaleY = static_cast<float>(srcH) / static_cast<float>(dstH);

		for (int32 y = 0; y < dstH; ++y)
		{
			const float fy = std::clamp((static_cast<float>(y) + 0.5f) * scaleY - 0.5f, 0.0f, static_cast<float>(srcH - 1));
			const int32 y0 = static_cast<int32>(fy);
			const int32 y1 = std::min(y0 + 1, srcH - 1);
			const float wy = fy - static_cast<float>(y0);

			for (int32 x = 0; x < dstW; ++x)
			{
				const float fx = std::clamp((static_cast<float>(x) + 0.5f) * scaleX - 0.5f, 0.0f, static_cast<float>(srcW - 1));
				const int32 x0 = static_cast<int32>(fx);
				const int32 x1 = std::min(x0 + 1, srcW - 1);
				const float wx = fx - static_cast<float>(x0);

				const size_t i00 = (static_cast<size_t>(y0) * srcW + x0) * 4;
				const size_t i10 = (static_cast<size_t>(y0) * srcW + x1) * 4;
				const size_t i01 = (static_cast<size_t>(y1) * srcW + x0) * 4;
				const size_t i11 = (static_cast<size_t>(y1) * srcW + x1) * 4;
				const size_t dstIdx = (static_cast<size_t>(y) * dstW + x) * 4;

				for (int c = 0; c < 4; ++c)
				{
					const float top = static_cast<float>(src[i00 + c]) * (1.0f - wx) + static_cast<float>(src[i10 + c]) * wx;
					const float bot = static_cast<float>(src[i01 + c]) * (1.0f - wx) + static_cast<float>(src[i11 + c]) * wx;
					outDst[dstIdx + c] = static_cast<uint8>(std::clamp(top * (1.0f - wy) + bot * wy + 0.5f, 0.0f, 255.0f));
				}
			}
		}
		return true;
	}
}

void ContentBrowserPanel::OpenSpriteSheetBakeModal(const filesystem::path& modelPath)
{
	m_SpriteBakeModelPath = modelPath;
	m_SpriteBakeFrameWidth = 64;
	m_SpriteBakeFrameHeight = 64;
	m_SpriteBakeDirections = 8;
	m_SpriteBakeFramesPerDirection = 1;
	m_SpriteBakeColumns = 8;
	m_SpriteBakePadding = 2;
	m_SpriteBakePivot = { 0.5f, 0.5f };
	m_SpriteBakeError.clear();
	m_OpenSpriteSheetBakeModal = true;

	filesystem::path modelStem = modelPath.stem();
	const std::string lowerName = Engine::WStrToStr(modelPath.filename().wstring());
	if (lowerName.size() >= 14 && _stricmp(lowerName.c_str() + lowerName.size() - 14, ".bammodel.json") == 0)
		modelStem.replace_extension();

	const filesystem::path defaultPngPath = modelPath.parent_path() / (modelStem.wstring() + L"_Sheet.png");
	const std::string defaultPngUtf8 = Engine::WStrToStr(defaultPngPath.wstring());
	const size_t copyLen = std::min(defaultPngUtf8.size(), sizeof(m_SpriteBakeOutputPngBuffer) - 1);
	memcpy(m_SpriteBakeOutputPngBuffer, defaultPngUtf8.data(), copyLen);
	m_SpriteBakeOutputPngBuffer[copyLen] = '\0';
}

void ContentBrowserPanel::DrawSpriteSheetBakeModal()
{
	constexpr const char* kPopupId = "BakeSpriteSheetPopup";
	const std::string modalTitle = LOCAL("ContentBrowser.BakeSpriteSheetTitle") + "###" + kPopupId;
	if (m_OpenSpriteSheetBakeModal)
	{
		ImGui::OpenPopup(modalTitle.c_str());
		m_OpenSpriteSheetBakeModal = false;
	}

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(540.0f, 0.0f), ImGuiCond_Appearing);
	ImGui::SetNextWindowSizeConstraints(ImVec2(540.0f, 0.0f), ImVec2(540.0f, FLT_MAX));

	bool open = true;
	ImVec4 modalBg = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
	modalBg.w = 1.0f;
	ImGui::PushStyleColor(ImGuiCol_PopupBg, modalBg);
	if (!ImGui::BeginPopupModal(modalTitle.c_str(), &open, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PopStyleColor();
		return;
	}

	if (EditorUI::BeginPropertyTable("SpriteSheetBakeTable"))
	{
		EditorUI::PropertyRow("ContentBrowser.BakeSpriteSheetFrameSize");
		int frameSize[2] = { m_SpriteBakeFrameWidth, m_SpriteBakeFrameHeight };
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputInt2("##SpriteBakeFrameSize", frameSize))
		{
			m_SpriteBakeFrameWidth = std::clamp(frameSize[0], 1, 4096);
			m_SpriteBakeFrameHeight = std::clamp(frameSize[1], 1, 4096);
		}

		EditorUI::PropertyRow("ContentBrowser.BakeSpriteSheetDirections");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputInt("##SpriteBakeDirections", &m_SpriteBakeDirections))
			m_SpriteBakeDirections = std::clamp(m_SpriteBakeDirections, 1, 64);

		EditorUI::PropertyRow("ContentBrowser.BakeSpriteSheetFramesPerDir");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputInt("##SpriteBakeFramesPerDir", &m_SpriteBakeFramesPerDirection))
			m_SpriteBakeFramesPerDirection = std::clamp(m_SpriteBakeFramesPerDirection, 1, 256);

		EditorUI::PropertyRow("ContentBrowser.BakeSpriteSheetColumns");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputInt("##SpriteBakeColumns", &m_SpriteBakeColumns))
			m_SpriteBakeColumns = std::clamp(m_SpriteBakeColumns, 0, 256);

		EditorUI::PropertyRow("ContentBrowser.BakeSpriteSheetPadding");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputInt("##SpriteBakePadding", &m_SpriteBakePadding))
			m_SpriteBakePadding = std::clamp(m_SpriteBakePadding, 0, 128);

		EditorUI::PropertyRow("ContentBrowser.BakeSpriteSheetPivot");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat2("##SpriteBakePivot", &m_SpriteBakePivot.x, 0.01f, 0.0f, 1.0f, "%.2f");

		EditorUI::PropertyRow("ContentBrowser.BakeSpriteSheetOutputPng");
		const float browseWidth = ImGui::CalcTextSize("...").x + ImGui::GetStyle().FramePadding.x * 2.0f + 12.0f;
		ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - browseWidth - ImGui::GetStyle().ItemSpacing.x));
		ImGui::InputText("##SpriteBakeOutputPng", m_SpriteBakeOutputPngBuffer, sizeof(m_SpriteBakeOutputPngBuffer));
		ImGui::SameLine();
		if (ImGui::Button("...##BrowseSpriteBakeOutput", ImVec2(browseWidth, 0.0f)))
		{
			const filesystem::path currentOut = Engine::StrToWStr(m_SpriteBakeOutputPngBuffer);
			std::wstring selectedPath;
			const std::vector<FileDialogFilter> filters = { { L"PNG Image (*.png)", L"*.png" } };
			if (FileDialogs::SaveFileDialog(selectedPath, filters,
				currentOut.filename().wstring(), L"png",
				currentOut.parent_path().wstring(), EFileDialogContext::General) && !selectedPath.empty())
			{
				const std::string utf8 = Engine::WStrToStr(selectedPath);
				const size_t copyLen = std::min(utf8.size(), sizeof(m_SpriteBakeOutputPngBuffer) - 1);
				memcpy(m_SpriteBakeOutputPngBuffer, utf8.data(), copyLen);
				m_SpriteBakeOutputPngBuffer[copyLen] = '\0';
			}
		}

		EditorUI::EndPropertyTable();
	}

	if (!m_SpriteBakeError.empty())
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", m_SpriteBakeError.c_str());
	}

	ImGui::Spacing();
	ImGui::Separator();
	const EditorUI::DialogAction actions[] = {
		{ "ContentBrowser.Cancel", true, false },
		{ "ContentBrowser.BakeSpriteSheetConfirm", true, false }
	};
	const int clicked = EditorUI::DialogButtonBar("SpriteSheetBakeFooter", actions, 2);
	if (clicked == 0)
	{
		m_SpriteBakeError.clear();
		ImGui::CloseCurrentPopup();
	}
	else if (clicked == 1)
	{
		const std::string outputPngUtf8(m_SpriteBakeOutputPngBuffer);
		if (outputPngUtf8.empty())
		{
			m_SpriteBakeError = LOCAL("ContentBrowser.BakeSpriteSheetInvalidPath");
		}
		else
		{
			auto& rm = Engine::ResourceManager::Get();
			Engine::Handle modelHandle = rm.FindHandleByFilePath(m_SpriteBakeModelPath);
			if (!modelHandle.IsValid())
				modelHandle = rm.LoadFile(m_SpriteBakeModelPath.wstring());
			auto* model = modelHandle.IsValid() ? dynamic_cast<Engine::Model*>(rm.GetResource(modelHandle)) : nullptr;

			if (!model)
			{
				m_SpriteBakeError = LOCAL("ContentBrowser.BakeSpriteSheetModelLoadFailed");
				NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
					m_SpriteBakeError, ELogLevel::Error, 4.0f);
			}
			else
			{
				SpriteSheetBakeConfig config;
				config.frameWidth = m_SpriteBakeFrameWidth;
				config.frameHeight = m_SpriteBakeFrameHeight;
				config.directions = m_SpriteBakeDirections;
				config.framesPerDirection = m_SpriteBakeFramesPerDirection;
				config.columns = m_SpriteBakeColumns;
				config.padding = m_SpriteBakePadding;
				config.pivot = m_SpriteBakePivot;
				config.outputPngPath = filesystem::path(Engine::StrToWStr(outputPngUtf8));
				config.outputSpriteMetaPath = config.outputPngPath;
				config.outputSpriteMetaPath.replace_extension(L".bamsprite.json");

				std::string captureError;
				const SpriteSheetBakeResult result = SpriteSheetBaker::BakeTurntableFromCapture(
					config,
					[model, &config, &captureError](int32, float, int32, int32, std::vector<uint8>& outRgbaFrame) -> bool
					{
						std::vector<uint8> captured;
						int32 srcW = 0;
						int32 srcH = 0;
						if (!SpriteSheetBaker::CaptureThumbnailFrameRGBA(model, captured, srcW, srcH, &captureError))
							return false;
						if (srcW == config.frameWidth && srcH == config.frameHeight)
						{
							outRgbaFrame = std::move(captured);
							return true;
						}
						if (!ResizeRGBA8Bilinear(captured, srcW, srcH, config.frameWidth, config.frameHeight, outRgbaFrame))
						{
							captureError = "Failed to resize captured thumbnail frame.";
							return false;
						}
						return true;
					});

				if (result.succeeded)
				{
					m_SpriteBakeError.clear();
					NotificationManager::Get().AddNotification(
						LOCAL("ContentBrowser.Title"),
						LOCAL_FMT("ContentBrowser.BakeSpriteSheetSuccess",
							Engine::WStrToStr(result.outputPngPath.filename().wstring()),
							result.atlasWidth, result.atlasHeight, result.totalFrames),
						ELogLevel::Info, 4.0f);
					RequestRefresh();
					ImGui::CloseCurrentPopup();
				}
				else
				{
					const std::string errDetail = !result.errorMessage.empty() ? result.errorMessage : captureError;
					m_SpriteBakeError = LOCAL_FMT("ContentBrowser.BakeSpriteSheetFailed", errDetail);
					NotificationManager::Get().AddNotification(
						LOCAL("ContentBrowser.Title"),
						m_SpriteBakeError, ELogLevel::Error, 4.0f);
				}
			}
		}
	}

	ImGui::EndPopup();
	ImGui::PopStyleColor();
}

#pragma region GridView
void ContentBrowserPanel::DrawDirectoryGrid()
{
	BackButton();
	EditorUI::SameLineIfFits(EditorUI::GetIconButtonSize(ICON_FA_ARROW_ROTATE_RIGHT).x);
	RefreshButton();
	EditorUI::SameLineIfFits(ImGui::GetFontSize() * 15.0f);
	const float rowWidth = ImGui::GetContentRegionAvail().x;
	const float filterWidth = std::min(ImGui::GetFontSize() * 9.0f, rowWidth * 0.45f);
	AddressBar(std::max(1.0f, rowWidth - filterWidth - ImGui::GetStyle().ItemSpacing.x));
	ImGui::SameLine();
	ContentFilter();
	bool enterPressed = SearchBar();
	ImGui::Separator();

	if (IsExternalUnregisteredDirectory(m_CurrentDirectory))
	{
		DrawExternalFolderNoticeBar();
	}

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

		if (m_LastWatchedDirectory != m_CurrentDirectory)
			RefreshCacheIfNeeded();

		DrawGrid(enterPressed);
		GridViewContextMenu(m_CurrentDirectory);
	}
	ImGui::EndChild();
}

void ContentBrowserPanel::DrawExternalFolderNoticeBar()
{
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.22f, 0.24f, 0.30f, 0.6f));
	const float barHeight = ImGui::GetFrameHeight() + 6.0f;
	if (ImGui::BeginChild("##ExternalNoticeBar", ImVec2(0.0f, barHeight), ImGuiChildFlags_Borders,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), ICON_FA_CIRCLE_INFO " %s",
			LOCAL_CSTR("ContentBrowser.ExternalReadOnlyNotice"));
		ImGui::SameLine();
		if (ImGui::Button((ICON_FA_LINK " " + LOCAL_LABEL("ContentBrowser.MountAsExternal")).c_str()))
		{
			OpenMountAddModal(m_CurrentDirectory);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", LOCAL_CSTR("ContentBrowser.MountAsExternalHint"));
		}
	}
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void ContentBrowserPanel::GridViewContextMenu(const filesystem::path& path)
{
	if (ImGui::BeginPopupContextWindow("GridContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
	{
		const bool isReadOnly = Engine::PlatformPath::IsReadOnlyTarget(path) || IsExternalUnregisteredDirectory(path);
		DrawFileClipboardMenu({}, path);
		ACTION_MENU_ITEM((ICON_FA_ROTATE_LEFT " " + LOCAL_LABEL("EditorCommon.Undo")).c_str(), "ContentBrowser.UndoFiles");
		ACTION_MENU_ITEM((ICON_FA_ROTATE_RIGHT " " + LOCAL_LABEL("EditorCommon.Redo")).c_str(), "ContentBrowser.RedoFiles");
		ImGui::Separator();

		if (IsExternalUnregisteredDirectory(m_CurrentDirectory))
		{
			if (ImGui::MenuItem((ICON_FA_LINK " " + LOCAL_LABEL("ContentBrowser.MountAsExternal")).c_str()))
			{
				OpenMountAddModal(m_CurrentDirectory);
			}
			ImGui::Separator();
		}

		ImGui::BeginDisabled(isReadOnly || !CanUseFileCommands());
		if (ImGui::BeginMenu((ICON_FA_PLUS " " + LOCAL_LABEL("EditorCommon.Create")).c_str()))
		{
			if (ImGui::MenuItem((ICON_FA_FOLDER " " + LOCAL_LABEL("EditorCommon.NewFolder")).c_str()))
			{
				CreateFolder(m_CurrentDirectory);
			}

			ImGui::Separator();

			if (ImGui::MenuItem((ICON_FA_GLOBE " " + LOCAL_LABEL("ContentBrowser.CreateEmptyMaterial")).c_str()))
			{
				CreateEmptyMaterial(m_CurrentDirectory);
			}

			if (ImGui::MenuItem((ICON_FA_FILE_CODE " " + LOCAL_LABEL("ContentBrowser.CreateMaterialInstance")).c_str()))
			{
				CreateEmptyMaterialInstance(m_CurrentDirectory);
			}

			ImGui::Separator();

			if (ImGui::MenuItem((ICON_FA_CLAPPERBOARD " " + LOCAL_LABEL("ContentBrowser.CreateScene")).c_str()))
			{
				CreateEmptyScene(m_CurrentDirectory);
			}

			if (ImGui::MenuItem((ICON_FA_BOX " " + LOCAL_LABEL("ContentBrowser.CreatePrefab")).c_str()))
			{
				CreateEmptyPrefab(m_CurrentDirectory);
			}

			ImGui::Separator();

			if (ImGui::MenuItem((ICON_FA_BOLT " " + LOCAL_LABEL("ContentBrowser.CreateGraph")).c_str()))
			{
				CreateEmptyGraph(m_CurrentDirectory);
			}

			if (ImGui::MenuItem((ICON_FA_STOPWATCH " " + LOCAL_LABEL("ContentBrowser.CreateTimeline")).c_str()))
			{
				CreateEmptyTimeline(m_CurrentDirectory);
			}

			if (ImGui::MenuItem((ICON_FA_LAYER_GROUP " " + LOCAL_LABEL("ContentBrowser.CreateTerrainLayer")).c_str()))
			{
				CreateEmptyTerrainLayer(m_CurrentDirectory);
			}

			if (ImGui::MenuItem((ICON_FA_FIRE " " + LOCAL_LABEL("ContentBrowser.CreateEffect")).c_str()))
			{
				CreateEmptyParticleEffect(m_CurrentDirectory);
			}

			ImGui::EndMenu();
		}
		ImGui::EndDisabled();

		ImGui::Separator();

		if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.ShowInExplorer")).c_str()))
		{
			ShellExecuteW(NULL, L"open", path.wstring().c_str(), NULL, NULL, SW_SHOWNORMAL);
		}

		ImGui::EndPopup();
	}
}

#pragma region Header Bar
void ContentBrowserPanel::BackButton()
{
	const auto& activeRoot = GetActiveRootForPath(m_CurrentDirectory);
	bool isRoot = (m_CurrentDirectory == activeRoot);
	if (!isRoot)
	{
		const auto parent = m_CurrentDirectory.parent_path();
		if (parent.empty() || parent == m_CurrentDirectory)
			isRoot = true;
	}

	EditorUI::IconButtonOptions options;
	options.Enabled = !isRoot;
	if (EditorUI::IconButton("ContentBack", ICON_FA_ARROW_UP, "ContentBrowser.ParentFolder", options))
	{
		if (!isRoot)
		{
			NavigateTo(m_CurrentDirectory.parent_path());
		}
	}
}

void ContentBrowserPanel::ApplyDirectory(const filesystem::path& directory)
{
	m_CurrentDirectory = directory;
	m_SearchBuffer[0] = '\0';
	m_NeedToExpandTree = true;
}

void ContentBrowserPanel::NavigateTo(const filesystem::path& directory)
{
	const auto next = directory.lexically_normal();
	if (next == m_CurrentDirectory)
		return;

	std::error_code error;
	if (!filesystem::is_directory(next, error))
		return;

	m_BackFolders.push_back(m_CurrentDirectory);
	m_ForwardFolders.clear();
	ApplyDirectory(next);
}

void ContentBrowserPanel::StepFolderHistory(bool forward)
{
	auto& from = forward ? m_ForwardFolders : m_BackFolders;
	auto& to = forward ? m_BackFolders : m_ForwardFolders;
	while (!from.empty())
	{
		const auto next = std::move(from.back());
		from.pop_back();

		std::error_code error;
		if (next == m_CurrentDirectory || !filesystem::is_directory(next, error))
			continue;
		if (!m_ShowEngineContent && Engine::PlatformPath::IsReadOnlyTarget(next))
			continue;

		to.push_back(m_CurrentDirectory);
		ApplyDirectory(next);
		return;
	}
}

void ContentBrowserPanel::RefreshButton()
{
	if (EditorUI::IconButton("ContentRefresh", ICON_FA_ARROW_ROTATE_RIGHT, "EditorCommon.Refresh"))
	{
		Editor::Detail::InvalidatePickerAssetCatalog();
		RequestRefresh();
	}
}

void ContentBrowserPanel::StartAddressBarEdit()
{
	m_AddressBarEditing = true;
	m_AddressBarSetFocus = true;
	const std::string utf8Path = Engine::WStrToStr(m_CurrentDirectory.lexically_normal().wstring());
	const size_t copyLen = std::min(utf8Path.size(), sizeof(m_AddressBarInputBuffer) - 1);
	memcpy(m_AddressBarInputBuffer, utf8Path.data(), copyLen);
	m_AddressBarInputBuffer[copyLen] = '\0';
}

void ContentBrowserPanel::NavigateToAddress(const char* inputBuffer)
{
	if (!inputBuffer)
		return;

	std::string rawInput = inputBuffer;
	const size_t start = rawInput.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return;
	const size_t end = rawInput.find_last_not_of(" \t\r\n");
	std::string trimmed = rawInput.substr(start, end - start + 1);

	if (trimmed.size() >= 2 && ((trimmed.front() == '"' && trimmed.back() == '"') ||
		(trimmed.front() == '\'' && trimmed.back() == '\'')))
	{
		trimmed = trimmed.substr(1, trimmed.size() - 2);
	}

	if (trimmed.empty())
		return;

	const std::wstring wideInput = Engine::StrToWStr(trimmed);
	filesystem::path targetPath;

	if (Engine::PlatformPath::IsVirtualUri(wideInput))
	{
		targetPath = Engine::PlatformPath::ResolveUri(wideInput);
	}
	else
	{
		filesystem::path rawPath(wideInput);
		std::error_code ec;
		if (rawPath.is_relative())
		{
			if (filesystem::exists(m_CurrentDirectory / rawPath, ec))
				targetPath = m_CurrentDirectory / rawPath;
			else if (filesystem::exists(m_RootPath / rawPath, ec))
				targetPath = m_RootPath / rawPath;
			else
				targetPath = rawPath;
		}
		else
		{
			targetPath = rawPath;
		}
	}

	if (targetPath.has_root_name() && !targetPath.has_root_directory() && targetPath.relative_path().empty())
	{
		targetPath += L"\\";
	}

	targetPath = targetPath.lexically_normal();

	std::error_code ec;
	if (filesystem::is_directory(targetPath, ec))
	{
		NavigateTo(targetPath);
	}
	else if (filesystem::is_regular_file(targetPath, ec))
	{
		NavigateTo(targetPath.parent_path());
		m_RevealedAssetPath = targetPath;
		m_RevealScrollPending = true;
	}
	else
	{
		NotificationManager::Get().AddNotification(
			LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.MountInvalidInput"),
			ELogLevel::Warn, 3.0f);
	}
}

void ContentBrowserPanel::AddressBar(float width)
{
	if (m_AddressBarEditing)
	{
		ImGui::SetNextItemWidth(width);
		if (m_AddressBarSetFocus)
		{
			ImGui::SetKeyboardFocusHere();
			m_AddressBarSetFocus = false;
		}

		const bool enterPressed = ImGui::InputText("##AddressBarInput", m_AddressBarInputBuffer, sizeof(m_AddressBarInputBuffer),
			ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

		if (enterPressed)
		{
			NavigateToAddress(m_AddressBarInputBuffer);
			m_AddressBarEditing = false;
		}
		else if (ImGui::IsKeyPressed(ImGuiKey_Escape))
		{
			m_AddressBarEditing = false;
		}
		else if (ImGui::IsItemDeactivated())
		{
			m_AddressBarEditing = false;
		}
		return;
	}

	const float editBtnWidth = ImGui::GetFrameHeight();
	const float barWidth = std::max(1.0f, width - editBtnWidth - ImGui::GetStyle().ItemSpacing.x);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
	ImGui::BeginChild("##AddressBarChild", ImVec2(barWidth, ImGui::GetFrameHeight()), ImGuiChildFlags_Borders,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 0.0f));

	const auto& activeRoot = GetActiveRootForPath(m_CurrentDirectory);
	const bool isEngineRoot = (!m_EngineRootPath.empty() && m_EngineRootPath != m_RootPath && activeRoot == m_EngineRootPath);
	const bool isEditorRoot = (!m_EditorRootPath.empty() && activeRoot == m_EditorRootPath);

	std::filesystem::path effectiveRoot = activeRoot;
	std::string rootDisplayName;

	if (isEngineRoot)
	{
		rootDisplayName = LOCAL("ContentBrowser.EngineResources");
	}
	else if (isEditorRoot)
	{
		rootDisplayName = LOCAL("ContentBrowser.EditorResources");
	}
	else
	{
		bool isMount = false;
		const auto contentMounts = Engine::PlatformPath::GetContentMounts();
		for (const auto& mount : contentMounts)
		{
			if (mount.RootPath.empty()) continue;
			if (Engine::PlatformPath::IsSamePath(m_CurrentDirectory, mount.RootPath) ||
				ContentBrowserPathWithin(m_CurrentDirectory, mount.RootPath))
			{
				effectiveRoot = mount.RootPath;
				rootDisplayName = std::string(ICON_FA_LINK " ") + mount.Name;
				isMount = true;
				break;
			}
		}

		if (!isMount)
		{
			if (IsExternalUnregisteredDirectory(m_CurrentDirectory))
			{
				std::filesystem::path cur = m_CurrentDirectory;
				while (!cur.empty() && cur != cur.parent_path())
				{
					cur = cur.parent_path();
				}
				effectiveRoot = cur.empty() ? m_CurrentDirectory.root_path() : cur;
				rootDisplayName = Engine::WStrToStr(effectiveRoot.wstring());
				if (rootDisplayName.empty())
					rootDisplayName = Engine::WStrToStr(m_CurrentDirectory.root_name().wstring());
			}
			else
			{
				rootDisplayName = LOCAL("ContentBrowser.Assets");
			}
		}
	}

	std::vector<std::filesystem::path> nodes;
	std::filesystem::path current = m_CurrentDirectory;
	while (current != effectiveRoot && !current.empty() && current != current.parent_path())
	{
		nodes.push_back(current);
		current = current.parent_path();
	}
	nodes.push_back(effectiveRoot);
	std::reverse(nodes.begin(), nodes.end());

	for (size_t i = 0; i < nodes.size(); ++i)
	{
		std::string name;
		if (i == 0)
		{
			name = rootDisplayName;
		}
		else
		{
			const auto fn = nodes[i].filename().wstring();
			name = Engine::WStrToStr(fn.empty() ? nodes[i].wstring() : fn);
		}
		name += "###AddressNode_" + std::to_string(static_cast<int>(i));

		if (ImGui::Button(name.c_str()))
		{
			NavigateTo(nodes[i]);
		}

		if (i < nodes.size() - 1)
		{
			ImGui::SameLine();
			ImGui::TextUnformatted(">");
			ImGui::SameLine();
		}
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleColor();

	const float remainingWidth = ImGui::GetContentRegionAvail().x;
	if (remainingWidth > 2.0f)
	{
		ImGui::SameLine();
		if (ImGui::InvisibleButton("##AddressBarEmptyArea", ImVec2(remainingWidth, ImGui::GetFrameHeight())))
		{
			StartAddressBarEdit();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
			ImGui::SetTooltip("%s", LOCAL_CSTR("ContentBrowser.AddressBarEditHint"));
		}
	}

	ImGui::EndChild();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();

	ImGui::SameLine();
	if (EditorUI::SmallIconButton("##AddressBarEditBtn", ICON_FA_PEN, "ContentBrowser.AddressBarEditHint"))
	{
		StartAddressBarEdit();
	}
}

bool ContentBrowserPanel::SearchBar()
{
	const char* scopeKey = m_SearchAllFolders ? "ContentBrowser.ScopeAll" : "ContentBrowser.ScopeCurrent";
	const string scopeLabel = LOCAL(scopeKey) + "###ContentSearchScope";
	const float scopeWidth = ImGui::CalcTextSize(scopeLabel.c_str(), nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2.0f;
	EditorUI::SearchBarOptions searchOptions;
	searchOptions.Width = std::max(1.0f, ImGui::GetContentRegionAvail().x - scopeWidth - ImGui::GetStyle().ItemSpacing.x);
	const auto search = EditorUI::DrawSearchBar("ContentSearch", m_SearchBuffer, sizeof(m_SearchBuffer),
		"EditorCommon.Search", searchOptions);
	bool enter = search.Submitted || search.Cleared;

	ImGui::SameLine();
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

	ImGui::SetNextItemWidth(-FLT_MIN);
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

void ContentBrowserPanel::DrawGridItem(const filesystem::directory_entry& directoryEntry, GridSelectionItem& item, ImDrawListSplitter* splitter)
{
	const auto& path = directoryEntry.path();

	ContentBrowserGridItem::TileRenderContext ctx;
	ctx.Visual = &item.Visual;
	ctx.IsReadOnly = item.IsReadOnly;
	ctx.ImGuiId = item.ImGuiIdStr.empty() ? nullptr : item.ImGuiIdStr.c_str();
	ctx.CachedTruncatedName = &item.TruncatedName;
	ctx.CachedTruncatedKey = &item.TruncatedKey;
	ctx.Splitter = splitter;

	if (path != m_RenamingPath && path != m_QueuedRenamePath &&
		ImGui::IsRectVisible(ImVec2(m_ThumbnailSize, m_ThumbnailSize)) && !item.IsDirectory)
	{
		const auto view = AssetCache::Get().GetThumbnailView(item.CacheKey, path);
		ctx.ThumbnailTexID = ImGuiManager::Get().GetImGuiTextureID(view.Texture);
		ctx.UV0 = view.UV0;
		ctx.UV1 = view.UV1;
		ctx.SpriteHandle = view.SpriteHandle;
	}

	const bool keepAlive = ContentBrowserGridItem::Draw(*this, directoryEntry, ctx,
		m_ThumbnailSize, item.Id, m_SelectedGridIds.contains(item.Id));

	if (keepAlive)
		m_InteractionPaths.push_back(path);
}

vector<filesystem::path> ContentBrowserPanel::GetInteractionPaths(ImGuiSelectionUserData itemId,
	const filesystem::path& path) const
{
	return m_SelectedGridIds.contains(itemId) ? GetSelectedFilePaths() : vector<filesystem::path>{ path };
}

vector<filesystem::path> ContentBrowserPanel::GetSelectedFilePaths() const
{
	vector<filesystem::path> paths;
	paths.reserve(m_SelectedGridIds.size());
	for (const auto& item : m_GridSelectionItems)
	{
		if (m_SelectedGridIds.contains(item.Id))
			paths.push_back(item.Path);
	}
	return paths;
}

bool ContentBrowserPanel::CanStoreFiles(const vector<filesystem::path>& paths, bool cut) const
{
	return m_Open && m_RenamingPath.empty() && PIEManager::Get().IsEditing() && !paths.empty() &&
		(!cut || std::ranges::none_of(paths, [](const auto& path)
			{ return Engine::PlatformPath::IsReadOnlyTarget(path); }));
}

bool ContentBrowserPanel::CanStoreSelectedFiles(bool cut) const
{
	return m_CurrentDirectory == m_LastWatchedDirectory && CanStoreFiles(GetSelectedFilePaths(), cut);
}

void ContentBrowserPanel::StoreSelectedFiles(bool cut)
{
	auto paths = GetSelectedFilePaths();
	if (m_CurrentDirectory == m_LastWatchedDirectory && CanStoreFiles(paths, cut))
	{
		m_FileClipboardPaths = std::move(paths);
		m_FileClipboardCut = cut;
	}
}

static void CheckContentBrowserEntry(const filesystem::path& path)
{
	const auto nativePath = filesystem::path(path).make_preferred();
	const DWORD attributes = GetFileAttributesW(nativePath.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		throw filesystem::filesystem_error("GetFileAttributesW", path,
			{ static_cast<int>(GetLastError()), std::system_category() });
	}
	if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
	{
		throw filesystem::filesystem_error("Reparse point", path,
			std::make_error_code(std::errc::operation_not_supported));
	}
}

static void CheckContentBrowserPath(const filesystem::path& path, const filesystem::path& root)
{
	for (auto current = path; ContentBrowserPathWithin(current, root); current = current.parent_path())
	{
		CheckContentBrowserEntry(current);
		if (ContentBrowserPathWithin(root, current))
			break;
	}
}

static bool IsBrowserCaseOnlyRename(const filesystem::path& source, const filesystem::path& target)
{
	return source.native() != target.native() &&
		CompareStringOrdinal(source.c_str(), -1, target.c_str(), -1, TRUE) == CSTR_EQUAL;
}

static void MoveContentBrowserEntry(const filesystem::path& source, const filesystem::path& target,
	bool* historyInvalid = nullptr)
{
	const auto nativeSource = filesystem::path(source).make_preferred();
	const auto nativeTarget = filesystem::path(target).make_preferred();
	if (IsBrowserCaseOnlyRename(nativeSource, nativeTarget))
	{
		const auto prefix = L".bam-rename-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
			std::to_wstring(GetTickCount64()) + L"-";
		for (size_t index = 0; index < 128; ++index)
		{
			const auto staging = nativeSource.parent_path() / (prefix + std::to_wstring(index));
			if (!MoveFileExW(nativeSource.c_str(), staging.c_str(), 0))
			{
				const DWORD error = GetLastError();
				if (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS)
					continue;
				throw filesystem::filesystem_error("Stage case rename", source, staging,
					{ static_cast<int>(error), std::system_category() });
			}
			if (MoveFileExW(staging.c_str(), nativeTarget.c_str(), 0))
				return;
			const DWORD error = GetLastError();
			if (!MoveFileExW(staging.c_str(), nativeSource.c_str(), 0))
			{
				if (historyInvalid)
					*historyInvalid = true;
				EDITOR_LOG_ERROR("Case rename rollback failed. Original asset remains at {}",
					Engine::WStrToStr(staging.wstring()));
			}
			throw filesystem::filesystem_error("Complete case rename", staging, target,
				{ static_cast<int>(error), std::system_category() });
		}
		throw filesystem::filesystem_error("No staging path for case rename", source,
			std::make_error_code(std::errc::file_exists));
	}
	if (!MoveFileExW(nativeSource.c_str(), nativeTarget.c_str(), 0))
	{
		throw filesystem::filesystem_error("MoveFileExW", source, target,
			{ static_cast<int>(GetLastError()), std::system_category() });
	}
}

static BY_HANDLE_FILE_INFORMATION ReadBrowserFileIdentity(const filesystem::path& path)
{
	const auto native = filesystem::path(path).make_preferred();
	const HANDLE handle = CreateFileW(native.c_str(), FILE_READ_ATTRIBUTES,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
		throw filesystem::filesystem_error("Open file for history", path,
			{ static_cast<int>(GetLastError()), std::system_category() });

	BY_HANDLE_FILE_INFORMATION result{};
	const BOOL read = GetFileInformationByHandle(handle, &result);
	const DWORD error = read ? ERROR_SUCCESS : GetLastError();
	CloseHandle(handle);
	if (!read)
		throw filesystem::filesystem_error("Read file identity", path, { static_cast<int>(error), std::system_category() });

	if ((result.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
		throw filesystem::filesystem_error("Reparse point", path, std::make_error_code(std::errc::operation_not_supported));
	return result;
}

static bool SameBrowserFile(const BY_HANDLE_FILE_INFORMATION& lhs, const BY_HANDLE_FILE_INFORMATION& rhs)
{
	return lhs.dwVolumeSerialNumber == rhs.dwVolumeSerialNumber &&
		lhs.nFileIndexHigh == rhs.nFileIndexHigh && lhs.nFileIndexLow == rhs.nFileIndexLow;
}

static bool BrowserPathHasSelectedParent(const filesystem::path& source, const vector<filesystem::path>& paths)
{
	return std::ranges::any_of(paths, [&source](const auto& candidate)
	{
		const auto parent = candidate.lexically_normal();
		return ContentBrowserPathWithin(source, parent) && !ContentBrowserPathWithin(parent, source);
	});
}

static void CheckBrowserTransferTree(const filesystem::path& source, const filesystem::path& target, bool move)
{
	const auto check = [move](const auto& from, const auto& to)
	{
		CheckContentBrowserEntry(from);
		if (Engine::PlatformPath::IsReadOnlyTarget(to) || (move && Engine::PlatformPath::IsReadOnlyTarget(from)))
			throw filesystem::filesystem_error("Read-only asset", from, to,
				std::make_error_code(std::errc::permission_denied));
	};

	check(source, target);
	if (filesystem::is_directory(source))
	{
		for (const auto& entry : filesystem::recursive_directory_iterator(source))
			check(entry.path(), target / entry.path().lexically_relative(source));
	}
}

static BY_HANDLE_FILE_INFORMATION CopyContentBrowserEntry(const filesystem::path& source, const filesystem::path& target)
{
	filesystem::path staging;
	for (int index = 0; index < 128; ++index)
	{
		const auto candidate = target.parent_path() / (L".bam-copy-tmp-" + std::to_wstring(index));
		std::error_code error;
		if (filesystem::create_directory(candidate, error))
		{
			staging = candidate;
			break;
		}
		if (error && error != std::errc::file_exists)
			throw filesystem::filesystem_error("Create copy staging directory", candidate, error);
	}
	if (staging.empty())
	{
		throw filesystem::filesystem_error("Reserve copy staging directory", target,
			std::make_error_code(std::errc::file_exists));
	}

	const auto cleanup = [&staging]()
		{
			std::error_code error;
			filesystem::remove_all(staging, error);
			if (error)
				EDITOR_LOG_WARN("Copy staging cleanup failed: {}", Engine::WStrToStr(staging.wstring()));
		};

	BY_HANDLE_FILE_INFORMATION identity{};
	try
	{
		const auto stagedEntry = staging / L"entry";
		filesystem::copy(source, stagedEntry, filesystem::copy_options::recursive);
		identity = ReadBrowserFileIdentity(stagedEntry);
		MoveContentBrowserEntry(stagedEntry, target);
	}
	catch (...)
	{
		cleanup();
		throw;
	}
	cleanup();
	return identity;
}

bool ContentBrowserPanel::CanTransferTo(const filesystem::path& directory) const
{
	return CanUseFileCommands() &&
		ContentBrowserPathWithin(directory.lexically_normal(), m_RootPath.lexically_normal()) &&
		!Engine::PlatformPath::IsReadOnlyTarget(directory);
}

bool ContentBrowserPanel::CanPasteFiles(const filesystem::path& directory) const
{
	return !m_FileClipboardPaths.empty() && CanTransferTo(directory);
}

void ContentBrowserPanel::PasteFiles(const filesystem::path& directory)
{
	if (!CanPasteFiles(directory))
		return;

	if (m_FileClipboardCut)
		QueueFileMove(m_FileClipboardPaths, directory, true);
	else
		TransferFiles(m_FileClipboardPaths, directory);
}

void ContentBrowserPanel::QueueFileMove(const vector<filesystem::path>& paths, const filesystem::path& directory,
	bool fromClipboard)
{
	if (paths.empty() || !m_PendingFileMove.empty() || !m_PendingFileDeletion.empty() ||
		m_DeletionPreparationPending)
		return;
	vector<AssetPathMove> moves;
	for (const auto& path : paths)
	{
		const auto source = path.lexically_normal();
		if (!BrowserPathHasSelectedParent(source, paths))
			moves.push_back({ source, directory.lexically_normal() / source.filename() });
	}
	QueueAssetPathChanges(moves, fromClipboard, false);
	if (!m_PendingFileMove.empty())
		m_MoveSourcePaths = paths;
}

void ContentBrowserPanel::QueueAssetPathChanges(const vector<AssetPathMove>& paths, bool fromClipboard, bool rename)
{
	if (paths.empty() || !m_PendingFileMove.empty() || !m_PendingFileDeletion.empty() ||
		m_MovePreparationPending || m_MovePreparation.valid() || m_DeletionPreparationPending)
		return;
	try
	{
		const auto destination = paths.front().After.parent_path().lexically_normal();
		if (!CanTransferTo(destination))
			return;
		CheckContentBrowserPath(destination, m_RootPath.lexically_normal());
		if (!filesystem::is_directory(destination))
			throw filesystem::filesystem_error("Move destination is not a directory", destination,
				std::make_error_code(std::errc::not_a_directory));
		const auto destinationIdentity = ReadBrowserFileIdentity(destination);
		vector<FileMove> moves;
		vector<filesystem::path> sources;
		const bool onlyEmptyDirectories = std::ranges::all_of(paths, [](const auto& change)
		{
			std::error_code error;
			if (!filesystem::is_directory(change.Before, error) || error)
				return false;
			const filesystem::directory_iterator entries(change.Before, error);
			return !error && entries == filesystem::directory_iterator{};
		});
		for (const auto& change : paths)
		{
			const auto source = change.Before.lexically_normal();
			const auto target = change.After.lexically_normal();
			if (!CanTransferTo(source.parent_path()) || target.parent_path() != destination ||
				ContentBrowserPathWithin(destination, source))
				throw filesystem::filesystem_error("Invalid move source or destination", source, target,
					std::make_error_code(std::errc::invalid_argument));
			CheckContentBrowserPath(source, m_RootPath.lexically_normal());
			moves.push_back({ source, target, ReadBrowserFileIdentity(source) });
			sources.push_back(source);
		}
		m_MoveSourcePaths = std::move(sources);
		m_MoveDestination = destination;
		m_MoveDestinationIdentity = destinationIdentity;
		m_MoveFromClipboard = fromClipboard;
		m_PendingRename = rename;
		m_PendingFileMove = std::move(moves);
		m_MoveImpact = {};
		m_MoveReferences.clear();
		m_MoveReferencesReady = false;
		m_MoveDocumentsAvailable = false;
		m_MoveBlock = {};
		m_MovePreparationCancelled = false;

		if (onlyEmptyDirectories)
		{
			const auto referenceStart = std::chrono::steady_clock::now();
			m_MoveReferencesReady = AssetReferenceValidator::PrepareMoveReferences(paths, m_MoveReferences, &m_MoveImpact);
			AssetManager::Get().RecordAssetReferenceTiming(
				AssetReferenceTimingKind::MovePreparation, ImGui::GetFrameCount(), referenceStart);
			m_MoveDocumentsAvailable = m_MoveReferencesReady &&
				CanApplyAssetMove(m_PendingFileMove, m_MoveReferences, &m_MoveBlock);
			if (rename && m_MoveReferencesReady && m_MoveDocumentsAvailable && m_MoveImpact.References.empty())
				ConfirmFileMove();
			return;
		}

		// Resolve lazy path roots on the editor thread before using them on a worker.
		PrimeAssetReferencePathRoots();
		m_MovePreparationPending = true;
		m_MovePreparation = std::async(std::launch::async, [paths]
		{
			MovePreparationResult result;
			const auto started = std::chrono::steady_clock::now();
			result.ReferencesReady = AssetReferenceValidator::PrepareMoveReferences(
				paths, result.References, &result.Impact);
			result.ElapsedMs = std::chrono::duration<double, std::milli>(
				std::chrono::steady_clock::now() - started).count();
			return result;
		});
	}
	catch (const filesystem::filesystem_error& error)
	{
		EDITOR_LOG_WARN("Could not prepare file move: {}", error.what());
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.FileHistoryFailed"), ELogLevel::Warn);
	}
	catch (const std::exception& error)
	{
		m_MovePreparationPending = false;
		m_MoveReferencesReady = false;
		m_MoveImpact.IsComplete = false;
		m_MoveDocumentsAvailable = false;
		EDITOR_LOG_WARN("Could not start asset move preparation: {}", error.what());
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.MoveReferenceScanFailed"), ELogLevel::Warn);
	}
}

void ContentBrowserPanel::ClearPendingFileMove()
{
	if (m_MovePreparationPending)
		m_MovePreparationCancelled = true;
	m_PendingFileMove.clear();
	m_MoveSourcePaths.clear();
	m_MoveDestination.clear();
	m_MoveDestinationIdentity = {};
	m_MoveFromClipboard = false;
	m_PendingRename = false;
	m_MoveImpact = {};
	m_MoveReferences.clear();
	m_MoveReferencesReady = false;
	m_MoveDocumentsAvailable = false;
	m_MoveBlock = {};
}

void ContentBrowserPanel::ConfirmFileMove()
{
	try
	{
		if (m_MovePreparationPending)
			return;
		CheckContentBrowserPath(m_MoveDestination, m_RootPath.lexically_normal());
		if (!SameBrowserFile(ReadBrowserFileIdentity(m_MoveDestination), m_MoveDestinationIdentity))
			throw filesystem::filesystem_error("Move destination changed after confirmation", m_MoveDestination,
				std::make_error_code(std::errc::state_not_recoverable));

		if (!m_MoveReferencesReady)
			throw filesystem::filesystem_error("Move reference preparation failed", m_MoveDestination,
				std::make_error_code(std::errc::state_not_recoverable));
		auto batch = std::make_shared<AssetMoveBatch>();
		batch->Moves = m_PendingFileMove;
		batch->References = m_MoveReferences;
		if (!batch->References.empty())
			batch->RecoveryDirectory = CreateFileHistoryBatch();
		const bool clearClipboard = m_MoveFromClipboard && m_FileClipboardCut && m_FileClipboardPaths == m_MoveSourcePaths;
		const char* label = m_PendingRename ? "ContentBrowser.FileRename" : "ContentBrowser.FileMove";
		auto command = std::make_unique<CallbackEditorCommand>(label,
			[this, batch]() { return ReplayAssetMove(*batch, true); },
			[this, batch]() { return ReplayAssetMove(*batch, false); });
		const bool succeeded = m_FileHistory.Execute(std::move(command));
		FinishFileHistoryOperation();
		if (succeeded && clearClipboard)
		{
			m_FileClipboardPaths.clear();
		}
		if (succeeded)
			NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
				LOCAL_FMT(m_PendingRename ? "ContentBrowser.RenameFilesResult" : "ContentBrowser.MoveFilesResult",
					batch->Moves.size(), batch->References.size()), ELogLevel::Info);
	}
	catch (const filesystem::filesystem_error& error)
	{
		EDITOR_LOG_WARN("Could not confirm file move: {}", error.what());
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.FileHistoryFailed"), ELogLevel::Warn);
	}
	ClearPendingFileMove();
}

void ContentBrowserPanel::DrawFileMoveConfirmation()
{
	if (m_PendingFileMove.empty())
		return;

	const char* titleKey = m_PendingRename ? "ContentBrowser.RenameFilesTitle" : "ContentBrowser.MoveFilesTitle";
	const auto title = LOCAL_LABEL(titleKey);
	if (!ImGui::IsPopupOpen(title.c_str()))
		ImGui::OpenPopup(title.c_str());
	if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::TextUnformatted(LOCAL_FMT(m_PendingRename ? "ContentBrowser.RenameFilesConfirm" : "ContentBrowser.MoveFilesConfirm",
		m_PendingFileMove.size()).c_str());
	DrawFileMovePaths();
	if (m_MovePreparationPending)
		ImGui::TextWrapped("%s", LOCAL_CSTR("ContentBrowser.MoveReferenceScanPending"));
	else
	{
		if (!m_MoveReferences.empty())
			ImGui::TextWrapped("%s", LOCAL_CSTR("ContentBrowser.MoveReferencesNotice"));
		DrawFileReferenceImpact(m_MoveImpact,
			m_PendingRename ? "ContentBrowser.RenameImpactCount" : "ContentBrowser.MoveImpactCount");
	}
	const bool documentsAvailable = m_MoveDocumentsAvailable;
	if (!m_MovePreparationPending && !m_MoveReferencesReady)
		ImGui::TextWrapped("%s", LOCAL_CSTR("ContentBrowser.MoveReferenceScanFailed"));
	if (!m_MovePreparationPending && !documentsAvailable)
		DrawFileMoveBlockReason();
	const EditorUI::DialogAction actions[] = {
		{ "EditorCommon.Cancel" },
		{ titleKey, !m_MovePreparationPending && CanTransferTo(m_MoveDestination) &&
			m_MoveReferencesReady && documentsAvailable }
	};
	const int action = EditorUI::DialogButtonBar("ContentBrowserMoveActions", actions, IM_ARRAYSIZE(actions));
	if (action >= 0)
	{
		ImGui::CloseCurrentPopup();
		if (action == 1)
			ConfirmFileMove();
		else
			ClearPendingFileMove();
	}
	ImGui::EndPopup();
}

void ContentBrowserPanel::DrawFileMovePaths() const
{
	const float width = std::min(ImGui::GetFontSize() * 42.0f, ImGui::GetMainViewport()->WorkSize.x * 0.85f);
	const float height = static_cast<float>(std::min<size_t>(m_PendingFileMove.size(), 4) + 1) *
		ImGui::GetFrameHeightWithSpacing();
	const auto flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame;
	if (!ImGui::BeginTable("MoveFilePaths", 2, flags, ImVec2(width, height)))
		return;

	ImGui::TableSetupColumn(LOCAL_LABEL("ContentBrowser.MoveSource").c_str());
	ImGui::TableSetupColumn(LOCAL_LABEL("ContentBrowser.MoveDestination").c_str());
	ImGui::TableHeadersRow();
	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(m_PendingFileMove.size()));
	while (clipper.Step())
	{
		for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const auto source = Engine::WStrToStr(Engine::PlatformPath::ToVirtualUri(m_PendingFileMove[index].Before));
			ImGui::TextUnformatted(source.c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", source.c_str());
			ImGui::TableSetColumnIndex(1);
			const auto target = Engine::WStrToStr(Engine::PlatformPath::ToVirtualUri(m_PendingFileMove[index].After));
			ImGui::TextUnformatted(target.c_str());
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", target.c_str());
		}
	}
	ImGui::EndTable();
}

vector<filesystem::path> ContentBrowserPanel::TransferFiles(const vector<filesystem::path>& paths,
	const filesystem::path& directory)
{
	const auto destination = directory.lexically_normal();
	vector<filesystem::path> failed;
	vector<FileMove> changes;
	changes.reserve(paths.size());
	filesystem::path copyBatch;
	size_t copySlot = 0;

	for (const auto& path : paths)
	{
		const auto source = path.lexically_normal();
		if (BrowserPathHasSelectedParent(source, paths))
			continue;

		try
		{
			const auto root = GetActiveRootForPath(source).lexically_normal();
			if (!CanTransferTo(destination) || !ContentBrowserPathWithin(source, root) ||
				ContentBrowserPathWithin(root, source) || ContentBrowserPathWithin(destination, source))
			{
				throw filesystem::filesystem_error("Invalid source or destination", source, destination,
					std::make_error_code(std::errc::invalid_argument));
			}

			CheckContentBrowserPath(source, root);
			CheckContentBrowserPath(destination, m_RootPath.lexically_normal());

			const bool isDirectory = filesystem::is_directory(source);
			auto target = destination / source.filename();
			if (ContentBrowserPathWithin(destination, source.parent_path()) &&
				ContentBrowserPathWithin(source.parent_path(), destination))
			{
				auto stem = isDirectory ? source.filename() : source.stem();
				auto extension = isDirectory ? wstring{} : source.extension().wstring();
				if (extension == L".json" && stem.has_extension())
				{
					extension = stem.extension().wstring() + extension;
					stem = stem.stem();
				}
				target = GenerateUniqueAssetPath(destination, stem.wstring(), extension);
			}

			CheckBrowserTransferTree(source, target, false);
			if (copyBatch.empty())
				copyBatch = CreateFileHistoryBatch();
			if (ReadBrowserFileIdentity(copyBatch).dwVolumeSerialNumber !=
				ReadBrowserFileIdentity(destination).dwVolumeSerialNumber)
			{
				throw filesystem::filesystem_error("File history on another volume", destination,
					std::make_error_code(std::errc::cross_device_link));
			}

			const auto slot = copyBatch / std::to_wstring(copySlot++);
			filesystem::create_directory(slot);
			FileMove entry{ slot / target.filename(), target, {} };
			entry.Identity = CopyContentBrowserEntry(source, target);
			changes.push_back(std::move(entry));
		}
		catch (const filesystem::filesystem_error& error)
		{
			failed.push_back(path);
			EDITOR_LOG_WARN("Asset transfer failed for {}: {}", Engine::WStrToStr(source.wstring()),
				error.code().message());
		}
	}

	const size_t succeeded = changes.size();
	if (succeeded > 0)
	{
		RequestRefresh();
		SubmitFileMoves(std::move(changes), "ContentBrowser.FileCopy", false);
	}

	const char* resultKey = "ContentBrowser.CopyFilesResult";
	NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
		LOCAL_FMT(resultKey, succeeded, failed.size()),
		failed.empty() ? ELogLevel::Info : ELogLevel::Warn, 5.0f);
	return failed;
}

filesystem::path ContentBrowserPanel::FileHistoryRoot() const
{
	return (Engine::PlatformPath::GetProjectRoot() / L".bam" / L"content-browser-history").lexically_normal();
}

filesystem::path ContentBrowserPanel::CreateFileHistoryBatch() const
{
	const auto project = Engine::PlatformPath::GetProjectRoot().lexically_normal();
	const auto root = FileHistoryRoot();
	if (ContentBrowserPathWithin(root, m_RootPath.lexically_normal()))
		throw filesystem::filesystem_error("History inside asset root", root, std::make_error_code(std::errc::invalid_argument));

	CheckContentBrowserEntry(project);
	filesystem::create_directory(root.parent_path());
	CheckContentBrowserEntry(root.parent_path());
	filesystem::create_directory(root);
	CheckContentBrowserEntry(root);

	const auto prefix = std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
	for (size_t index = 0;; ++index)
	{
		const auto batch = root / (prefix + L"-" + std::to_wstring(index));
		if (filesystem::create_directory(batch))
			return batch;
	}
}

bool ContentBrowserPanel::CanApplyAssetMove(const vector<FileMove>& moves,
	const vector<AssetReferenceEdit>& references, AssetMoveBlock* outBlock) const
{
	AssetPathRelocation relocation;
	return PrepareAssetMove(moves, references, true, relocation, outBlock);
}

void ContentBrowserPanel::DrawFileMoveBlockReason() const
{
	// 참조 스캔이 실패한 경우에는 MoveReferenceScanFailed 문구가 이미 원인을 설명한다.
	if (!m_MoveBlock)
	{
		if (m_MoveReferencesReady)
			ImGui::TextWrapped("%s", LOCAL_CSTR("ContentBrowser.MoveDocumentConflict"));
		return;
	}

	const char* key = "ContentBrowser.MoveDocumentConflict";
	switch (m_MoveBlock.Reason)
	{
	case EAssetMoveBlockReason::NotEditing: key = "ContentBrowser.MoveBlockedNotEditing"; break;
	case EAssetMoveBlockReason::DocumentBusy: key = "ContentBrowser.MoveBlockedDocumentBusy"; break;
	case EAssetMoveBlockReason::SceneLoading: key = "ContentBrowser.MoveBlockedSceneLoading"; break;
	case EAssetMoveBlockReason::PrefabMode: key = "ContentBrowser.MoveBlockedPrefabMode"; break;
	case EAssetMoveBlockReason::UnsavedResourceEditor: key = "ContentBrowser.MoveBlockedUnsavedEditor"; break;
	case EAssetMoveBlockReason::UnsavedDocument: key = "ContentBrowser.MoveBlockedUnsavedDocument"; break;
	case EAssetMoveBlockReason::DocumentRejected: key = "ContentBrowser.MoveBlockedDocumentRejected"; break;
	case EAssetMoveBlockReason::EditorRetainsPath: key = "ContentBrowser.MoveBlockedEditorRetainsPath"; break;
	case EAssetMoveBlockReason::None: break;
	}

	// 대상 이름이 없는 문구에도 같은 인자를 넘긴다. fmt는 사용하지 않은 추가 인자를 무시한다.
	ImGui::TextWrapped("%s", LOCAL_FMT(key, m_MoveBlock.Subject).c_str());
}

bool ContentBrowserPanel::PrepareAssetMove(const vector<FileMove>& moves, const vector<AssetReferenceEdit>& references,
	bool forward, AssetPathRelocation& relocation, AssetMoveBlock* outBlock) const
{
	vector<Engine::FilePathChange> changes;
	vector<filesystem::path> documents;
	for (const auto& move : moves)
		changes.push_back({ forward ? move.Before : move.After, forward ? move.After : move.Before });
	for (const auto& reference : references)
	{
		documents.push_back(reference.BeforePath);
		documents.push_back(reference.AfterPath);
	}
	relocation = AssetPathRelocation(std::move(changes), std::move(documents));
	return ImGuiManager::Get().PrepareAssetFileChanges(relocation, outBlock);
}

void ContentBrowserPanel::RemapBrowserPaths(const vector<FileMove>& moves, bool forward)
{
	vector<Engine::FilePathChange> changes;
	for (const auto& move : moves)
		changes.push_back({ forward ? move.Before : move.After, forward ? move.After : move.Before });
	const auto remap = [&changes](const filesystem::path& path)
	{
		return Engine::PlatformPath::RemapFilePath(path, changes);
	};
	m_CurrentDirectory = remap(m_CurrentDirectory);
	m_LastDirectory = remap(m_LastDirectory);
	m_RevealedAssetPath = remap(m_RevealedAssetPath);
	for (auto* paths : { &m_BackFolders, &m_ForwardFolders, &m_FileClipboardPaths, &m_ContextMenuPaths })
	{
		for (auto& path : *paths)
			path = remap(path);
	}
	if (moves.size() == 1)
	{
		const auto& target = forward ? moves.front().After : moves.front().Before;
		if (m_CurrentDirectory == target.parent_path())
		{
			m_RevealedAssetPath = target;
			m_RevealScrollPending = true;
		}
	}
	m_LastSearchString.clear();
	m_SearchResults.clear();
	m_NeedToExpandTree = true;
	RequestRefresh();
}

bool ContentBrowserPanel::ReplayAssetMove(const AssetMoveBatch& batch, bool forward)
{
	const auto fail = [this](const char* key)
	{
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"), LOCAL(key), ELogLevel::Warn);
		return false;
	};
	AssetPathRelocation relocation;
	if (!PrepareAssetMove(batch.Moves, batch.References, forward, relocation))
		return fail("ContentBrowser.MoveDocumentConflict");
	if (!AssetReferenceValidator::ValidateMoveReferences(batch.References, forward))
		return fail("ContentBrowser.MoveReferenceChanged");

	// Detect documents added since confirmation or the preceding Undo/Redo, before any file changes.
	vector<filesystem::path> sources;
	for (const auto& move : batch.Moves)
		sources.push_back(forward ? move.Before : move.After);
	const auto referenceStart = std::chrono::steady_clock::now();
	const auto impact = AssetReferenceValidator::AnalyzeMove(sources);
	AssetManager::Get().RecordAssetReferenceTiming(
		AssetReferenceTimingKind::MoveRecheck, ImGui::GetFrameCount(), referenceStart);
	if (!impact.IsComplete || std::ranges::any_of(impact.References, [&](const auto& link)
	{
		return std::ranges::none_of(batch.References, [&](const auto& edit)
		{
			std::error_code error;
			return filesystem::equivalent(link.Source, forward ? edit.BeforePath : edit.AfterPath, error) && !error;
		});
	}))
		return fail("ContentBrowser.MoveReferenceChanged");

	bool attemptedFiles = false;
	const bool success = Engine::ResourceManager::Get().RelocateFileResources(relocation.Changes(), [&]()
	{
		// The ResourceManager lock is held here. Do not call resource APIs or apply editor callbacks.
		attemptedFiles = true;
		if (!ReplayFileMoves(batch.Moves, forward)) return false;
		const auto result = AssetReferenceValidator::ApplyMoveReferences(batch.References, forward, batch.RecoveryDirectory);
		if (result == EAssetReferenceWriteResult::Success) return true;
		const bool pathsRestored = ReplayFileMoves(batch.Moves, !forward);
		if (!pathsRestored || result == EAssetReferenceWriteResult::RollbackFailed)
		{
			m_FileHistoryInvalid = true;
			EDITOR_LOG_ERROR("Asset move rollback failed. Reference backups: {}",
				Engine::WStrToStr(batch.RecoveryDirectory.wstring()));
			return fail("ContentBrowser.FileHistoryRollbackFailed");
		}
		return fail("ContentBrowser.MoveReferenceChanged");
	});
	if (!success)
		return attemptedFiles ? false : fail("ContentBrowser.MoveLiveStateConflict");
	// Resource indices now refer to the new locations and their lock has been released.
	relocation.Apply();
	RemapBrowserPaths(batch.Moves, forward);
	return true;
}

bool ContentBrowserPanel::ReplayFileMoves(const vector<FileMove>& moves, bool forward)
{
	const auto assetRoot = m_RootPath.lexically_normal();
	const auto historyRoot = FileHistoryRoot();
	const auto endpoints = [forward](const FileMove& move)
	{
		using PathRef = const filesystem::path&;
		return std::pair<PathRef, PathRef>(forward ? move.Before : move.After, forward ? move.After : move.Before);
	};

	const auto validateParent = [&](const filesystem::path& path)
	{
		const auto parent = path.parent_path();
		if (ContentBrowserPathWithin(parent, assetRoot) && CanTransferTo(parent))
			CheckContentBrowserPath(parent, assetRoot);
		else if (ContentBrowserPathWithin(parent, historyRoot))
			CheckContentBrowserPath(parent, Engine::PlatformPath::GetProjectRoot().lexically_normal());
		else
			throw filesystem::filesystem_error("Invalid file history path", path,
				std::make_error_code(std::errc::permission_denied));
	};

	size_t applied = 0;
	try
	{
		for (const auto& move : moves)
		{
			const auto [source, target] = endpoints(move);
			validateParent(source);
			validateParent(target);
			CheckBrowserTransferTree(source, target, true);
			if (!SameBrowserFile(ReadBrowserFileIdentity(source), move.Identity))
				throw filesystem::filesystem_error("File history source was replaced", source,
					std::make_error_code(std::errc::state_not_recoverable));
			if (filesystem::exists(filesystem::symlink_status(target)) &&
				!(IsBrowserCaseOnlyRename(source, target) && SameBrowserFile(ReadBrowserFileIdentity(target), move.Identity)))
				throw filesystem::filesystem_error("File history target exists", target,
					std::make_error_code(std::errc::file_exists));
		}

		for (const auto& move : moves)
		{
			const auto [source, target] = endpoints(move);
			if (!SameBrowserFile(ReadBrowserFileIdentity(source), move.Identity))
				throw filesystem::filesystem_error("File history source was replaced", source,
					std::make_error_code(std::errc::state_not_recoverable));
			MoveContentBrowserEntry(source, target, &m_FileHistoryInvalid);
			++applied;
		}
	}
	catch (const filesystem::filesystem_error& error)
	{
		EDITOR_LOG_WARN("File history failed: {}", error.what());
		while (applied > 0)
		{
			const auto& move = moves[--applied];
			const auto [source, target] = endpoints(move);
			try
			{
				validateParent(source);
				validateParent(target);
				if (!SameBrowserFile(ReadBrowserFileIdentity(target), move.Identity))
					throw filesystem::filesystem_error("Rollback file was replaced", target,
						std::make_error_code(std::errc::state_not_recoverable));
				MoveContentBrowserEntry(target, source, &m_FileHistoryInvalid);
			}
			catch (const filesystem::filesystem_error& rollbackError)
			{
				m_FileHistoryInvalid = true;
				EDITOR_LOG_ERROR("File history rollback failed: {}", rollbackError.what());
			}
		}

		RequestRefresh();
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL(m_FileHistoryInvalid ? "ContentBrowser.FileHistoryRollbackFailed" : "ContentBrowser.FileHistoryFailed"),
			ELogLevel::Warn);
		return false;
	}

	RequestRefresh();
	return true;
}

void ContentBrowserPanel::FinishFileHistoryOperation()
{
	if (std::exchange(m_FileHistoryInvalid, false))
		m_FileHistory.Clear();
}

void ContentBrowserPanel::SubmitFileMoves(vector<FileMove> moves, const char* label, bool execute)
{
	if (moves.empty())
		return;

	const auto batch = std::make_shared<const vector<FileMove>>(std::move(moves));
	const auto replay = [this, batch](bool forward)
	{
		if (!ReplayFileMoves(*batch, forward))
			return false;
		RequestAssetReferenceScanRefresh();
		return true;
	};
	auto command = std::make_unique<CallbackEditorCommand>(label,
		[replay]() { return replay(true); },
		[replay]() { return replay(false); });

	if (execute)
		m_FileHistory.Execute(std::move(command));
	else
	{
		m_FileHistory.Record(std::move(command));
		RequestAssetReferenceScanRefresh();
	}
	FinishFileHistoryOperation();
}

bool ContentBrowserPanel::CanUseFileCommands() const
{
	return m_Open && m_RenamingPath.empty() && !m_MovePreparationPending &&
		!m_DeletionPreparationPending && PIEManager::Get().IsEditing();
}

bool ContentBrowserPanel::CanUndoFileEdit() const
{
	return CanUseFileCommands() && m_FileHistory.CanUndo();
}

bool ContentBrowserPanel::CanRedoFileEdit() const
{
	return CanUseFileCommands() && m_FileHistory.CanRedo();
}

void ContentBrowserPanel::UndoFileEdit()
{
	if (CanUndoFileEdit())
		m_FileHistory.Undo();
	FinishFileHistoryOperation();
}

void ContentBrowserPanel::RedoFileEdit()
{
	if (CanRedoFileEdit())
		m_FileHistory.Redo();
	FinishFileHistoryOperation();
}

bool ContentBrowserPanel::CanDeleteFiles(const vector<filesystem::path>& paths) const
{
	return !paths.empty() && std::ranges::all_of(paths, [this](const auto& path)
	{
		return CanTransferTo(path.parent_path()) && !Engine::PlatformPath::IsReadOnlyTarget(path);
	});
}

bool ContentBrowserPanel::CanDeleteSelectedFiles() const
{
	return m_CurrentDirectory == m_LastWatchedDirectory && CanDeleteFiles(GetSelectedFilePaths());
}

void ContentBrowserPanel::DeleteSelectedFiles()
{
	if (m_CurrentDirectory == m_LastWatchedDirectory)
		QueueFileDeletion(GetSelectedFilePaths());
}

void ContentBrowserPanel::QueueFileDeletion(const vector<filesystem::path>& paths)
{
	if (!CanDeleteFiles(paths) || m_DeletionPreparationPending || m_DeletionPreparation.valid())
		return;

	const auto resetPreparation = [this]
	{
		m_DeletionPreparationPending = false;
		m_DeletionPreparationCancelled = false;
		m_DeletionPreparation = {};
		m_PendingFileDeletion.clear();
		m_DeletionImpact = {};
	};
	try
	{
		vector<FileMove> moves;
		moves.reserve(paths.size());
		for (const auto& path : paths)
		{
			const auto source = path.lexically_normal();
			if (BrowserPathHasSelectedParent(source, paths))
				continue;

			CheckContentBrowserPath(source, m_RootPath.lexically_normal());
			moves.push_back({ source, {}, ReadBrowserFileIdentity(source) });
		}
		if (moves.empty())
			return;

		vector<filesystem::path> targets;
		targets.reserve(moves.size());
		for (const auto& move : moves)
			targets.push_back(move.Before);

		PrimeAssetReferencePathRoots();
		m_DeletionImpact = {};
		m_DeletionPreparationCancelled = false;
		m_PendingFileDeletion = std::move(moves);
		std::promise<DeletionPreparationResult> completion;
		m_DeletionPreparation = completion.get_future();
		m_DeletionPreparationPending = true;
		m_DeletionPreparationWorker = std::jthread(
			[targets = std::move(targets), completion = std::move(completion)]() mutable
		{
			using Clock = std::chrono::steady_clock;
			using Milliseconds = std::chrono::duration<double, std::milli>;
			const auto started = Clock::now();
			DeletionPreparationResult result;
			try
			{
				result.Impact = AssetReferenceValidator::AnalyzeDeletion(targets);
			}
			catch (...)
			{
				result.Error = std::current_exception();
				result.Impact.IsComplete = false;
			}
			result.ElapsedMs = Milliseconds(Clock::now() - started).count();
			completion.set_value(std::move(result));
		});
	}
	catch (const filesystem::filesystem_error& error)
	{
		resetPreparation();
		EDITOR_LOG_WARN("Could not capture file deletion targets: {}", error.what());
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.FileHistoryFailed"), ELogLevel::Warn);
	}
	catch (const std::exception& error)
	{
		resetPreparation();
		EDITOR_LOG_WARN("Could not start file deletion impact analysis: {}", error.what());
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.DeleteImpactScanFailed"), ELogLevel::Warn);
	}
}

void ContentBrowserPanel::ProcessDeletionPreparation()
{
	if (!m_DeletionPreparationPending)
		return;
	if (!m_Open)
	{
		m_DeletionPreparationCancelled = true;
		m_PendingFileDeletion.clear();
		m_DeletionImpact = {};
	}
	if (!m_DeletionPreparation.valid() ||
		m_DeletionPreparation.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		return;

	if (m_DeletionPreparationWorker.joinable())
		m_DeletionPreparationWorker.join();
	m_DeletionPreparationPending = false;
	try
	{
		auto result = m_DeletionPreparation.get();
		AssetManager::Get().RecordBackgroundAssetReferenceTiming(
			AssetReferenceTimingKind::DeleteImpact, result.ElapsedMs);
		const bool cancelled = std::exchange(m_DeletionPreparationCancelled, false);
		if (cancelled)
		{
			m_PendingFileDeletion.clear();
			m_DeletionImpact = {};
			return;
		}

		if (result.Error)
		{
			try
			{
				std::rethrow_exception(result.Error);
			}
			catch (const std::exception& error)
			{
				EDITOR_LOG_WARN("Could not analyze file deletion impact: {}", error.what());
			}
			catch (...)
			{
				EDITOR_LOG_WARN("Could not analyze file deletion impact: unknown worker exception");
			}
			m_PendingFileDeletion.clear();
			m_DeletionImpact = {};
			NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
				LOCAL("ContentBrowser.DeleteImpactScanFailed"), ELogLevel::Warn);
			return;
		}

		m_DeletionImpact = std::move(result.Impact);
	}
	catch (const std::exception& error)
	{
		m_DeletionPreparationCancelled = false;
		m_PendingFileDeletion.clear();
		m_DeletionImpact = {};
		EDITOR_LOG_WARN("Could not collect file deletion impact analysis: {}", error.what());
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.DeleteImpactScanFailed"), ELogLevel::Warn);
	}
	catch (...)
	{
		m_DeletionPreparationCancelled = false;
		m_PendingFileDeletion.clear();
		m_DeletionImpact = {};
		EDITOR_LOG_WARN("Could not collect file deletion impact analysis: unknown worker exception");
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.DeleteImpactScanFailed"), ELogLevel::Warn);
	}
}

void ContentBrowserPanel::DeleteFiles(vector<FileMove> moves)
{
	try
	{
		const auto batch = CreateFileHistoryBatch();
		for (size_t index = 0; index < moves.size(); ++index)
		{
			const auto slot = batch / std::to_wstring(index);
			filesystem::create_directory(slot);
			moves[index].After = slot / moves[index].Before.filename();
		}
		SubmitFileMoves(std::move(moves), "ContentBrowser.FileDelete", true);
	}
	catch (const filesystem::filesystem_error& error)
	{
		EDITOR_LOG_WARN("Could not prepare file deletion: {}", error.what());
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.FileHistoryFailed"), ELogLevel::Warn);
	}
}

void ContentBrowserPanel::DrawFileDeletionConfirmation()
{
	if (m_PendingFileDeletion.empty())
		return;

	const auto title = LOCAL_LABEL("EditorCommon.Delete");
	if (!ImGui::IsPopupOpen(title.c_str()))
		ImGui::OpenPopup(title.c_str());
	if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::TextUnformatted(LOCAL_FMT("ContentBrowser.DeleteFilesConfirm", m_PendingFileDeletion.size()).c_str());
	if (m_DeletionPreparationPending)
		ImGui::TextWrapped("%s", LOCAL_CSTR("ContentBrowser.DeleteReferenceScanPending"));
	else
		DrawFileReferenceImpact(m_DeletionImpact, "ContentBrowser.DeleteImpactCount");
	const EditorUI::DialogAction actions[] = {
		{ "EditorCommon.Cancel" }, { "EditorCommon.Delete", CanUseFileCommands(), true }
	};
	const int action = EditorUI::DialogButtonBar("ContentBrowserDeleteActions", actions, IM_ARRAYSIZE(actions));
	if (action >= 0)
	{
		auto moves = std::exchange(m_PendingFileDeletion, vector<FileMove>{});
		m_DeletionImpact = {};
		ImGui::CloseCurrentPopup();
		if (action == 1)
			DeleteFiles(std::move(moves));
		else
			m_DeletionPreparationCancelled = m_DeletionPreparationPending;
	}
	ImGui::EndPopup();
}

void ContentBrowserPanel::DrawFileReferenceImpact(const AssetReferenceImpact& impact, const char* countKey) const
{
	if (!impact.IsComplete)
		ImGui::TextWrapped("%s", LOCAL_CSTR("ContentBrowser.DeleteImpactIncomplete"));

	const auto& references = impact.References;
	if (references.empty())
	{
		if (impact.IsComplete)
			ImGui::TextWrapped("%s", LOCAL_CSTR("ContentBrowser.DeleteImpactNone"));
		return;
	}

	ImGui::TextUnformatted(LOCAL_FMT(countKey, references.size()).c_str());
	const float width = std::min(ImGui::GetFontSize() * 42.0f, ImGui::GetMainViewport()->WorkSize.x * 0.85f);
	const float height = static_cast<float>(std::min<size_t>(references.size(), 6) + 1) * ImGui::GetFrameHeightWithSpacing();
	const auto flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame;
	if (!ImGui::BeginTable("DeletionImpactReferences", 2, flags, ImVec2(width, height)))
		return;

	ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.SourceAsset").c_str());
	ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.TargetReference").c_str());
	ImGui::TableHeadersRow();

	const auto drawPath = [](const filesystem::path& path)
	{
		const auto display = Engine::WStrToStr(Engine::PlatformPath::ToVirtualUri(path));
		ImGui::TextUnformatted(display.c_str());
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", display.c_str());
	};
	ImGuiListClipper clipper;
	clipper.Begin(static_cast<int>(references.size()));
	while (clipper.Step())
	{
		for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			drawPath(references[index].Source);
			ImGui::TableSetColumnIndex(1);
			drawPath(references[index].Target);
		}
	}
	ImGui::EndTable();
}

void ContentBrowserPanel::DrawFileClipboardMenu(const vector<filesystem::path>& paths,
	const filesystem::path& directory)
{
	auto& shortcuts = ShortcutManager::Get();
	if (!paths.empty())
	{
		if (ImGui::MenuItem((ICON_FA_COPY " " + LOCAL_LABEL("EditorCommon.Copy")).c_str(),
			shortcuts.GetShortcutHint("ContentBrowser.CopyFiles"), false, CanStoreFiles(paths, false)))
		{
			m_FileClipboardPaths = paths;
			m_FileClipboardCut = false;
		}

		if (ImGui::MenuItem((ICON_FA_SCISSORS " " + LOCAL_LABEL("EditorCommon.Cut")).c_str(),
			shortcuts.GetShortcutHint("ContentBrowser.CutFiles"), false, CanStoreFiles(paths, true)))
		{
			m_FileClipboardPaths = paths;
			m_FileClipboardCut = true;
		}

		if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str(),
			shortcuts.GetShortcutHint("ContentBrowser.DeleteFiles"), false, CanDeleteFiles(paths)))
		{
			QueueFileDeletion(paths);
		}
	}

	if (ImGui::MenuItem((ICON_FA_PASTE " " + LOCAL_LABEL("EditorCommon.Paste")).c_str(),
		shortcuts.GetShortcutHint("ContentBrowser.PasteFiles"), false, CanPasteFiles(directory)))
	{
		PasteFiles(directory);
	}
}

void ContentBrowserPanel::RebuildGridItems(const vector<filesystem::directory_entry>& source)
{
	std::unordered_map<filesystem::path, ImGuiSelectionUserData> previousIds;
	previousIds.reserve(m_GridSelectionItems.size());
	for (auto& item : m_GridSelectionItems)
		previousIds.emplace(std::move(item.Path), item.Id);

	m_GridSelectionItems.clear();
	m_GridSelectionItems.reserve(source.size());
	m_GridSelectionIndices.clear();

	const auto& activeRoot = GetActiveRootForPath(m_CurrentDirectory);
	const bool hideEngineContent = !m_ShowEngineContent &&
		activeRoot != m_EngineRootPath && activeRoot != m_EditorRootPath;
	const bool isExternalDir = IsExternalUnregisteredDirectory(m_CurrentDirectory);

	for (size_t index = 0; index < source.size(); ++index)
	{
		const auto& entry = source[index];
		const auto& path = entry.path();

		auto readOnlyIt = m_ReadOnlyCache.find(path.native());
		if (readOnlyIt == m_ReadOnlyCache.end())
			readOnlyIt = m_ReadOnlyCache.emplace(path.native(), Engine::PlatformPath::IsReadOnlyTarget(path)).first;
		const bool isReadOnly = readOnlyIt->second || isExternalDir;

		if ((!isExternalDir && hideEngineContent && readOnlyIt->second) || !PassesFilter(entry, m_FilterMask))
			continue;

		const auto previous = previousIds.find(path);
		const auto id = previous != previousIds.end() ? previous->second : m_NextGridSelectionId++;
		const bool isDir = entry.is_directory();

		GridSelectionItem item{};
		item.Path = path;
		item.Id = id;
		item.SourceIndex = index;
		item.IsDirectory = isDir;
		item.IsReadOnly = isReadOnly;
		item.CacheKey = isDir ? std::string{} : AssetCache::BuildCacheKey(path);
		item.ImGuiIdStr = Engine::WStrToStr(path.wstring());
		item.Visual = ContentBrowserGridItem::DescribeAssetVisual(path, isDir);

		m_GridSelectionIndices.emplace(id, static_cast<int>(m_GridSelectionItems.size()));
		m_GridSelectionItems.push_back(std::move(item));
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
	const string searchStr = enterPressed || m_SearchBuffer[0] == '\0' ? string(m_SearchBuffer) : m_LastSearchString;
	const bool isSearching = !searchStr.empty();
	const bool searchCleared = !isSearching && !m_LastSearchString.empty();

	if (enterPressed || (isSearching && m_GridItemsDirty))
	{
		m_LastSearchString = searchStr;
		m_SearchResults.clear();

		std::wstring searchQuery;
		try
		{
			searchQuery = Engine::StrToWStr(searchStr);
		}
		catch (const std::filesystem::filesystem_error&)
		{
			EDITOR_LOG_WARN("Content Browser search query could not be decoded as UTF-8.");
		}

		if (!searchQuery.empty())
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
					if (StringContainsCaseInsensitive(entry.path().filename().wstring(), searchQuery))
						m_SearchResults.push_back(entry);
				}

				if (!m_EngineRootPath.empty() && m_EngineRootPath != m_RootPath &&
					std::filesystem::is_directory(m_EngineRootPath, searchEc))
				{
					for (const auto& entry : filesystem::recursive_directory_iterator(
						m_EngineRootPath, filesystem::directory_options::skip_permission_denied, searchEc))
					{
						if (StringContainsCaseInsensitive(entry.path().filename().wstring(), searchQuery))
							m_SearchResults.push_back(entry);
					}
				}

				if (!m_EditorRootPath.empty() && m_EditorRootPath != m_RootPath && m_EditorRootPath != m_EngineRootPath &&
					std::filesystem::is_directory(m_EditorRootPath, searchEc))
				{
					for (const auto& entry : filesystem::recursive_directory_iterator(
						m_EditorRootPath, filesystem::directory_options::skip_permission_denied, searchEc))
					{
						if (StringContainsCaseInsensitive(entry.path().filename().wstring(), searchQuery))
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
					if (StringContainsCaseInsensitive(entry.path().filename().wstring(), searchQuery))
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

	if (m_GridItemsDirty || enterPressed || m_GridUsesSearch != isSearching ||
		m_GridFilterMask != m_FilterMask || m_GridShowsEngineContent != m_ShowEngineContent ||
		m_CurrentDirectory != m_LastWatchedDirectory)
	{
		RebuildGridItems(source);
		m_GridUsesSearch = isSearching;
		m_GridFilterMask = m_FilterMask;
		m_GridShowsEngineContent = m_ShowEngineContent;
		m_GridItemsDirty = false;
	}

	const int itemCount = static_cast<int>(m_GridSelectionItems.size());
	int revealIndex = -1;
	if (m_RevealScrollPending)
	{
		for (int index = 0; index < itemCount; ++index)
		{
			if (m_GridSelectionItems[index].Path.lexically_normal() == m_RevealedAssetPath)
			{
				revealIndex = index;
				m_SelectedGridIds.clear();
				m_SelectedGridIds.insert(m_GridSelectionItems[index].Id);
				break;
			}
		}

		if (revealIndex < 0)
			m_RevealScrollPending = false;
	}

	// 타일은 마우스를 뗄 때 선택하여 타일에서 시작하는 기존 드롭과 구분한다.
	const ImGuiMultiSelectFlags selectionFlags = ImGuiMultiSelectFlags_ScopeWindow | ImGuiMultiSelectFlags_BoxSelect2d |
		ImGuiMultiSelectFlags_ClearOnClickVoid | ImGuiMultiSelectFlags_SelectOnClickRelease |
		ImGuiMultiSelectFlags_NoSelectAll | ImGuiMultiSelectFlags_NoSelectOnRightClick;
	const auto* selection = ImGui::BeginMultiSelect(selectionFlags, static_cast<int>(m_SelectedGridIds.size()), itemCount);
	ApplyGridSelectionRequests(*selection);
	const auto rangeSource = m_GridSelectionIndices.find(selection->RangeSrcItem);

	if (itemCount == 0)
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
			EditorUI::EmptyState("ContentBrowser.EmptyFolder");
		}
		ApplyGridSelectionRequests(*ImGui::EndMultiSelect());
		return;
	}

	const float cellSize = m_ThumbnailSize + m_Padding;
	const float panelWidth = ImGui::GetContentRegionAvail().x;
	const int columnCount = std::clamp(static_cast<int>(panelWidth / cellSize), 1, 64);
	const int rowCount = (itemCount + columnCount - 1) / columnCount;

	// 이름 변경 입력창도 같은 행 높이 안에 들어가도록 최소 높이를 확보합니다.
	const float nameHeight = ContentBrowserGridItem::GetNameBoxHeight();
	const float rowHeight = std::ceil(m_ThumbnailSize + nameHeight + m_Padding);
	const ImVec2 gridStartScreen = ImGui::GetCursorScreenPos();
	const float startX = gridStartScreen.x + m_Padding * 0.5f;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	ImDrawListSplitter splitter;
	splitter.Split(drawList, 3);

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
			const auto& path = m_GridSelectionItems[index].Path;
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
			const float rowStartY = gridStartScreen.y + static_cast<float>(row) * rowHeight;

			for (int column = 0; column < columnCount; ++column)
			{
				const int index = row * columnCount + column;
				if (index >= itemCount)
					break;

				const ImVec2 tileMin(startX + static_cast<float>(column) * cellSize, rowStartY + m_Padding * 0.5f);
				ImGui::SetCursorScreenPos(tileMin);

				auto& item = m_GridSelectionItems[index];
				DrawGridItem(source[item.SourceIndex], item, &splitter);

				if (!m_RevealedAssetPath.empty() && item.Path.lexically_normal() == m_RevealedAssetPath)
				{
					splitter.SetCurrentChannel(drawList, 2);
					const ImVec2 tileMax(tileMin.x + m_ThumbnailSize, tileMin.y + m_ThumbnailSize);
					drawList->AddRect(tileMin, tileMax, ImGui::GetColorU32(ImGuiCol_CheckMark), 4.0f);
					splitter.SetCurrentChannel(drawList, 0);

					if (index == revealIndex)
					{
						ImGui::SetScrollHereY(0.5f);
						m_RevealScrollPending = false;
					}
				}
			}
		}
	}

	splitter.Merge(drawList);
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
			ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", Engine::WStrToStr(conflict.Dest.filename().wstring()).c_str());
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

			const EditorUI::DialogAction actions[] = {
				{ "ContentBrowser.Skip" }, { "ContentBrowser.Overwrite", true, true }, { "ContentBrowser.CreateCopy" }
			};
			const int action = EditorUI::DialogButtonBar("FileConflictActions", actions, IM_ARRAYSIZE(actions));
			if (action == 1)
				processConflict(1);
			else if (action == 2)
				processConflict(2);
			else if (action == 0)
				ImGui::CloseCurrentPopup();
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
	if (!filesystem::is_regular_file(absolutePath, error) && !filesystem::is_directory(absolutePath, error))
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
	NavigateTo(m_RevealedAssetPath.parent_path());
	m_RevealScrollPending = true;
	m_NeedToExpandTree = true;

	m_SearchBuffer[0] = '\0';
	m_LastSearchString.clear();
	m_SearchResults.clear();
	m_AddressBarEditing = false;
	m_AddressBarInputBuffer[0] = '\0';
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
		m_ReadOnlyCache.clear();
	}

	if (!fileSystemChanged && !directoryChanged)
	{
		return;
	}

	m_LastWatchedDirectory = m_CurrentDirectory;
	m_GridItemsDirty = true;
	m_CachedEntries.clear();

	std::error_code ec;
	if (std::filesystem::exists(m_CurrentDirectory, ec))
	{
		for (auto& directoryEntry : filesystem::directory_iterator(m_CurrentDirectory, ec))
		{
			m_CachedEntries.push_back(directoryEntry);
		}

		// 폴더 우선 및 자연 정렬(Natural Sort, StrCmpLogicalW)을 적용하여 1, 2, 10 순으로 정렬
		std::sort(m_CachedEntries.begin(), m_CachedEntries.end(), [](const filesystem::directory_entry& a, const filesystem::directory_entry& b) {
			const bool aIsDir = a.is_directory();
			const bool bIsDir = b.is_directory();
			if (aIsDir != bIsDir) return aIsDir > bIsDir;
			return StrCmpLogicalW(a.path().filename().c_str(), b.path().filename().c_str()) < 0;
		});
	}
	else
	{
		// 현재 디렉토리가 외부에서 통째로 날아갔다면 루트 에셋 폴더로 강제 복귀
		m_CurrentDirectory = m_RootPath;
		m_LastWatchedDirectory.clear();
		m_CachedSubdirectories.clear();
		m_ReadOnlyCache.clear();
	}
}
void ContentBrowserPanel::OnExteranalDropped(const vector<string>& droppedFiles)
{
	if (Engine::PlatformPath::IsReadOnlyTarget(m_CurrentDirectory) || IsExternalUnregisteredDirectory(m_CurrentDirectory))
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
			const bool isTexture = extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
				extension == ".tga" || extension == ".bmp";
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
				if (isTexture)
					AssetManager::Get().ImportAsync(sourcePath, destinationPath);
				else
					AssetManager::Get().Import(sourcePath, destinationPath);
			}
		}
	}
}
void ContentBrowserPanel::CreateFolder(const filesystem::path& directory)
{
	if (!CanTransferTo(directory))
		return;
	try
	{
		CheckContentBrowserPath(directory, m_RootPath.lexically_normal());
		const auto path = GenerateUniqueAssetPath(directory, L"NewFolder", L"");
		if (filesystem::create_directory(path))
			QueueCreatedAssetRename(path);
	}
	catch (const filesystem::filesystem_error& error)
	{
		EDITOR_LOG_WARN("Folder creation failed: {}", error.what());
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL("ContentBrowser.FileHistoryFailed"), ELogLevel::Warn);
	}
}

filesystem::path ContentBrowserPanel::GenerateUniqueAssetPath(const filesystem::path& directory, const wstring& baseStem, const wstring& extension)
{
	filesystem::path candidate = directory / (baseStem + extension);
	int suffix = 1;
	while (filesystem::exists(candidate))
	{
		candidate = directory / (baseStem + L"_" + std::to_wstring(suffix++) + extension);
	}
	return candidate;
}

namespace
{
	// 새 에셋 파일에 프로젝트 상대 경로를 기록한다. 불러올 때 로더가 다시 지정하므로 파일 값은 참고용이다.
	void AssignNewAssetIdentity(Engine::Resource& resource, const filesystem::path& outputPath)
	{
		const wstring relativePath = Engine::NormalizePath(Engine::PlatformPath::ToProjectRelative(outputPath).wstring());
		resource.SetKey(relativePath);
		resource.SetPath(relativePath);
	}
}

void ContentBrowserPanel::CreateEmptyMaterial(const filesystem::path& path)
{
	if (!CanTransferTo(path))
		return;
	const filesystem::path outputPath = GenerateUniqueAssetPath(path, L"NewMaterial", L".bammat.json");
	Material* material = Engine::Material::CreateEmpty();

	if (!material)
		return;

	auto& resourceManager = ResourceManager::Get();
	AssignNewAssetIdentity(*material, outputPath);
	const EResult result = resourceManager.SaveToJsonFile(material, outputPath.wstring());
	resourceManager.DestroyResource(material);

	if (result == EResult::Success)
	{
		RequestRefresh();
		QueueCreatedAssetRename(outputPath);
	}
	else
	{
		EDITOR_LOG_WARN("Material saving is unavailable: {}", ResultToString(result));
	}
}

void ContentBrowserPanel::CreateEmptyMaterialInstance(const filesystem::path& path)
{
	if (!CanTransferTo(path))
		return;
	const filesystem::path outputPath = GenerateUniqueAssetPath(path, L"NewMaterialInstance", L".bammatinst.json");
	auto& rm = ResourceManager::Get();
	const wstring baseMatKey = L"Resources/Material/DefaultMaterial.bammat.json";
	auto baseHandle = rm.GetResourceHandle<Material>(baseMatKey);
	if (!baseHandle)
	{
		baseHandle = ResourceHandle<Material>(rm.LoadFile(baseMatKey));
	}

	MaterialInstance* instance = MaterialInstance::Create(baseHandle);
	if (!instance)
		return;

	AssignNewAssetIdentity(*instance, outputPath);
	const EResult result = rm.SaveToJsonFile(instance, outputPath.wstring());
	rm.DestroyResource(instance);

	if (result == EResult::Success)
	{
		RequestRefresh();
		QueueCreatedAssetRename(outputPath);
	}
	else
	{
		EDITOR_LOG_WARN("MaterialInstance saving is unavailable: {}", ResultToString(result));
	}
}

void ContentBrowserPanel::CreateEmptyScene(const filesystem::path& path)
{
	if (!CanTransferTo(path))
		return;
	const filesystem::path outputPath = GenerateUniqueAssetPath(path, L"NewScene", L".bamscene.json");
	const std::string templateJson = R"({
    "Scene": {
        "m_Flags": 1,
        "m_Layers": [
            {
                "__Type__": "Layer",
                "m_Flags": 3,
                "m_Index": 0,
                "m_Name": "Default"
            }
        ]
    }
})";

	std::ofstream file(outputPath, std::ios::out | std::ios::trunc);
	if (file.is_open())
	{
		file << templateJson;
		file.close();
		if (!file)
		{
			EDITOR_LOG_WARN("New asset write failed: {}", Engine::WStrToStr(outputPath.wstring()));
			return;
		}
		RequestRefresh();
		QueueCreatedAssetRename(outputPath);
	}
	else
	{
		EDITOR_LOG_WARN("Scene creation failed: could not open file {}", outputPath.string());
	}
}

void ContentBrowserPanel::CreateEmptyPrefab(const filesystem::path& path)
{
	if (!CanTransferTo(path))
		return;
	const filesystem::path outputPath = GenerateUniqueAssetPath(path, L"NewPrefab", L".bamprefab.json");
	const std::string templateJson = R"({
    "Prefab": {
        "Entities": [
            {
                "Name": "NewPrefab",
                "Components": [
                    {
                        "Type": "Engine::FlagComponent",
                        "Data": { "flags": 3 }
                    },
                    {
                        "Type": "Engine::TransformComponent",
                        "Data": {
                            "position": [0.0, 0.0, 0.0],
                            "rotation": [1.0, 0.0, 0.0, 0.0],
                            "scale": [1.0, 1.0, 1.0]
                        }
                    }
                ]
            }
        ]
    }
})";

	std::ofstream file(outputPath, std::ios::out | std::ios::trunc);
	if (file.is_open())
	{
		file << templateJson;
		file.close();
		if (!file)
		{
			EDITOR_LOG_WARN("New asset write failed: {}", Engine::WStrToStr(outputPath.wstring()));
			return;
		}
		RequestRefresh();
		QueueCreatedAssetRename(outputPath);
	}
	else
	{
		EDITOR_LOG_WARN("Prefab creation failed: could not open file {}", outputPath.string());
	}
}

void ContentBrowserPanel::CreateEmptyGraph(const filesystem::path& path)
{
	if (!CanTransferTo(path))
		return;
	const filesystem::path outputPath = GenerateUniqueAssetPath(path, L"NewGraph", L".bamgraph.json");
	const std::string templateJson = R"({
    "Graph": {
        "Domain": 0,
        "NextLinkId": 1,
        "NextNodeId": 2,
        "Version": 1,
        "Nodes": [
            {
                "Id": 1,
                "Type": "Event.Update",
                "InputValues": []
            }
        ],
        "Links": []
    }
})";

	std::ofstream file(outputPath, std::ios::out | std::ios::trunc);
	if (file.is_open())
	{
		file << templateJson;
		file.close();
		if (!file)
		{
			EDITOR_LOG_WARN("New asset write failed: {}", Engine::WStrToStr(outputPath.wstring()));
			return;
		}
		RequestRefresh();
		QueueCreatedAssetRename(outputPath);
	}
	else
	{
		EDITOR_LOG_WARN("Graph creation failed: could not open file {}", outputPath.string());
	}
}

void ContentBrowserPanel::CreateEmptyTimeline(const filesystem::path& path)
{
	if (!CanTransferTo(path))
		return;
	const filesystem::path outputPath = GenerateUniqueAssetPath(path, L"NewTimeline", L".bamtimeline.json");

	// 손으로 쓴 JSON 템플릿은 필드가 바뀌면 읽기에 실패한다(이전 템플릿은 없는 트랙 필드와 빠진 필수 필드로 열리지 않았다).
	// 빈 에셋을 리플렉션으로 저장해 현재 구조와 항상 일치시킨다. 모델·클립은 타임라인 편집기에서 지정한다.
	auto* asset = Engine::ActionTimelineAsset::CreateEmpty();
	if (!asset)
		return;
	// 길이 3초는 편집 범위다. 새 타임라인은 클립·트랙이 끝나는 시점에 끝나므로, 빈 공간 때문에 다음 재생이 늦어지지 않는다.
	asset->SetDuration(3.0f);
	asset->SetEndPolicy(Engine::ETimelineEndPolicy::LastTrackEnd);
	const bool saved = asset->SaveToFile(Engine::WStrToStr(outputPath.wstring()));
	Engine::ResourceManager::Get().DestroyResource(asset);
	if (!saved)
	{
		EDITOR_LOG_WARN("Timeline creation failed: could not write file {}", Engine::WStrToStr(outputPath.wstring()));
		return;
	}
	RequestRefresh();
	QueueCreatedAssetRename(outputPath);
}

void ContentBrowserPanel::CreateEmptyTerrainLayer(const filesystem::path& path)
{
	if (!CanTransferTo(path))
		return;
	const filesystem::path outputPath = GenerateUniqueAssetPath(path, L"NewTerrainLayer", Engine::TerrainLayerAsset::FileSuffix);

	// 타임라인과 같이 빈 에셋을 리플렉션으로 저장해 현재 구조와 항상 일치시킨다. 값은 지형 레이어 편집기에서 바꾼다.
	auto* asset = Engine::TerrainLayerAsset::CreateEmpty();
	if (!asset)
		return;
	const bool saved = asset->SaveToFile(outputPath);
	Engine::ResourceManager::Get().DestroyResource(asset);
	if (!saved)
	{
		EDITOR_LOG_WARN("Terrain layer creation failed: could not write file {}", Engine::WStrToStr(outputPath.wstring()));
		return;
	}
	RequestRefresh();
	QueueCreatedAssetRename(outputPath);
}

void ContentBrowserPanel::CreateEmptyParticleEffect(const filesystem::path& path)
{
	if (!CanTransferTo(path))
		return;
	const filesystem::path outputPath = GenerateUniqueAssetPath(path, L"NewParticleEffect", L".bameffect.json");
	const std::string templateJson = R"({
    "AssetHeader": {
        "MagicNumber": 6072351289621954882,
        "ResourceType": 17,
        "Version": 0
    },
    "m_Emitters": [
        {
            "emitterName": "NewEmitter",
            "shapeType": 0,
            "spawnRate": 30.0,
            "particleLifetime": 1.5,
            "speedMin": 1.0,
            "speedMax": 3.0,
            "sizeStart": [0.2, 0.2, 0.2],
            "sizeEnd": [0.0, 0.0, 0.0],
            "colorStart": [1.0, 0.8, 0.2, 1.0],
            "colorEnd": [1.0, 0.1, 0.0, 0.0]
        }
    ]
})";

	std::ofstream file(outputPath, std::ios::out | std::ios::trunc);
	if (file.is_open())
	{
		file << templateJson;
		file.close();
		if (!file)
		{
			EDITOR_LOG_WARN("New asset write failed: {}", Engine::WStrToStr(outputPath.wstring()));
			return;
		}
		RequestRefresh();
		QueueCreatedAssetRename(outputPath);
	}
	else
	{
		EDITOR_LOG_WARN("ParticleEffect creation failed: could not open file {}", outputPath.string());
	}
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
	m_ShowEngineContent = workspace.GetSplitter("ContentBrowser.ShowEngineContent", 1.0f) > 0.5f;
	RestoreViewState(workspace.ContentBrowserDirectory, workspace.ContentBrowserThumbnailSize);
}
#pragma endregion


void ContentBrowserPanel::RestoreViewState(const std::string& relativeDirectory, float thumbnailSize)
{
	m_ThumbnailSize = std::isfinite(thumbnailSize) && thumbnailSize >= 32.0f && thumbnailSize <= 256.0f
		? thumbnailSize : 64.0f;

	m_BackFolders.clear();
	m_ForwardFolders.clear();
	m_CurrentDirectory = m_RootPath;
	m_LastDirectory.clear();
	m_LastWatchedDirectory.clear();
	m_NeedToExpandTree = true;
	RequestRefresh();

	m_SearchBuffer[0] = '\0';
	m_LastSearchString.clear();
	m_SearchResults.clear();
	m_AddressBarEditing = false;
	m_AddressBarInputBuffer[0] = '\0';

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

void ContentBrowserPanel::DrawPathCopyMenu(const vector<filesystem::path>& paths)
{
	string pathText;
	string uriText;
	bool canCopyPath = !paths.empty();
	bool canCopyUri = !paths.empty();

	for (const auto& path : paths)
	{
		std::error_code error;
		auto absolutePath = filesystem::absolute(path, error);
		if (error)
		{
			canCopyPath = false;
			canCopyUri = false;
			break;
		}

		absolutePath = absolutePath.lexically_normal();
		const auto uri = Engine::PlatformPath::ToVirtualUri(absolutePath);
		canCopyUri = canCopyUri && Engine::PlatformPath::IsVirtualUri(uri);

		if (!pathText.empty())
		{
			pathText += '\n';
			uriText += '\n';
		}

		pathText += Engine::WStrToStr(absolutePath.make_preferred().wstring());
		uriText += Engine::WStrToStr(uri);
	}

	if (ImGui::MenuItem((ICON_FA_COPY " " + LOCAL_LABEL("EditorCommon.CopyPath")).c_str(),
		nullptr, false, canCopyPath))
	{
		ImGui::SetClipboardText(pathText.c_str());
	}

	if (ImGui::MenuItem((ICON_FA_LINK " " + LOCAL_LABEL("EditorCommon.CopyUri")).c_str(),
		nullptr, false, canCopyUri))
	{
		ImGui::SetClipboardText(uriText.c_str());
	}
}

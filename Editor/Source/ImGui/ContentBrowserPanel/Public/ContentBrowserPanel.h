#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include <efsw/efsw.hpp>
#include <atomic>
#include <exception>
#include <future>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include "ModelImporter.h"
#include "PreviewViewportWidget.h"
#include "CommandHistory.h"
#include "AssetReferenceValidator.h"
#include "ContentBrowserGridItem.h"

BEGIN(Editor)
class AssetPathRelocation;

class ContentBrowserFileListener : public efsw::FileWatchListener
{
public:
	explicit ContentBrowserFileListener(std::atomic<bool>& refreshFlag) : m_RefreshFlag(refreshFlag) {}
	bool HasReferenceChanges() const { return m_HasReferenceChanges.load(); }
	AssetReferenceChanges TakeReferenceChanges();
	void SetProjectWatchId(efsw::WatchID watchId) { m_ProjectWatchId = watchId; }
	void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename, efsw::Action action, std::string oldFilename) override;

private:
	std::atomic<bool>& m_RefreshFlag;
	void QueueReferenceChanges(AssetReferenceChanges changes);
	std::mutex m_ReferenceChangesMutex;
	AssetReferenceChanges m_ReferenceChanges;
	std::atomic<bool> m_HasReferenceChanges{ false };
	efsw::WatchID m_ProjectWatchId = 0;
};

struct FileDropConflict
{
	filesystem::path Source;
	filesystem::path Dest;
};

class ContentBrowserPanel : public IImGui
{
	friend class ContentBrowserGridItem;

public:
	ContentBrowserPanel() = default;
	virtual ~ContentBrowserPanel() = default;
	virtual void Update(f32 dt) override;
	bool ShouldUpdateWhenClosed() const override
	{
		return m_MovePreparationPending || m_DeletionPreparationPending ||
			(m_FileListener && m_FileListener->HasReferenceChanges());
	}
	virtual void Draw() override;
	virtual void ProcessEvent(const SDL_Event* event) override;
	virtual void Free() override;
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::ContentBrowser; }
	void SetRenameTarget(const filesystem::path& path, bool inTree = false)
	{
		m_RenameTargetPath = path;
		m_RenameTargetInTree = inTree;
	}
	bool CanRenameTarget() const;
	void RenameTarget();

#pragma region Request
public:
	void RequestRefresh() { m_NeedsCacheRefresh = true; }
	void RestoreViewState(const std::string& relativeDirectory, float thumbnailSize);
	std::string GetRelativeDirectory() const;
	float GetThumbnailSize() const { return m_ThumbnailSize; }
	bool RequestRevealAsset(const filesystem::path& resourcePath);
	bool IsShowingEngineContent() const { return m_ShowEngineContent; }
	void SetShowEngineContent(bool show);
	const filesystem::path& GetEngineRootPath() const { return m_EngineRootPath; }
	const filesystem::path& GetEditorRootPath() const { return m_EditorRootPath; }
	const filesystem::path& GetActiveRootForPath(const filesystem::path& path) const;
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Assets; }
#pragma endregion

private:
	EResult OnInitialize(void* arg) override;
	void BeginRename(const filesystem::path& path, bool inTree);
	void DrawRenameEditor(const char* id, const filesystem::path& path);
	void QueueCreatedAssetRename(const filesystem::path& path);
	void ProcessQueuedRename();
	void CreateFolder(const filesystem::path& directory);

#pragma region File Actions
	friend class ContentBrowserTreeItem;

public:
	bool CanStoreSelectedFiles(bool cut) const;
	void StoreSelectedFiles(bool cut);
	bool CanPasteFiles(const filesystem::path& directory) const;
	void PasteFiles(const filesystem::path& directory);
	const filesystem::path& GetCurrentDirectory() const { return m_CurrentDirectory; }
	void NavigateTo(const filesystem::path& directory);
	bool CanDeleteSelectedFiles() const;
	void DeleteSelectedFiles();
	bool CanUndoFileEdit() const;
	bool CanRedoFileEdit() const;
	void UndoFileEdit();
	void RedoFileEdit();

private:
	vector<filesystem::path> GetSelectedFilePaths() const;
	bool CanStoreFiles(const vector<filesystem::path>& paths, bool cut) const;
	bool CanTransferTo(const filesystem::path& directory) const;
	void DrawFileClipboardMenu(const vector<filesystem::path>& paths, const filesystem::path& directory);
	void DrawPathCopyMenu(const vector<filesystem::path>& paths);
	void DrawFolderDropTarget(const filesystem::path& directory);
	void ApplyDirectory(const filesystem::path& directory);
	void StepFolderHistory(bool forward);

	struct FileMove
	{
		filesystem::path Before;
		filesystem::path After;
		BY_HANDLE_FILE_INFORMATION Identity;
	};

	vector<filesystem::path> TransferFiles(const vector<filesystem::path>& paths, const filesystem::path& directory);
	struct AssetMoveBatch
	{
		vector<FileMove> Moves;
		vector<AssetReferenceEdit> References;
		filesystem::path RecoveryDirectory;
	};
	struct MovePreparationResult
	{
		bool ReferencesReady = false;
		double ElapsedMs = 0.0;
		AssetReferenceImpact Impact;
		vector<AssetReferenceEdit> References;
	};
	struct DeletionPreparationResult
	{
		double ElapsedMs = 0.0;
		AssetReferenceImpact Impact;
		std::exception_ptr Error;
	};
	bool ReplayAssetMove(const AssetMoveBatch& batch, bool forward);
	bool CanApplyAssetMove(const vector<FileMove>& moves, const vector<AssetReferenceEdit>& references) const;
	bool PrepareAssetMove(const vector<FileMove>& moves, const vector<AssetReferenceEdit>& references,
		bool forward, AssetPathRelocation& relocation) const;
	void QueueFileMove(const vector<filesystem::path>& paths, const filesystem::path& directory, bool fromClipboard);
	void QueueAssetPathChanges(const vector<AssetPathMove>& moves, bool fromClipboard, bool rename);
	void RemapBrowserPaths(const vector<FileMove>& moves, bool forward);
	void DrawFileMoveConfirmation();
	void DrawFileMovePaths() const;
	void ConfirmFileMove();
	void ClearPendingFileMove();
	void ProcessMovePreparation();
	bool CanUseFileCommands() const;
	bool CanDeleteFiles(const vector<filesystem::path>& paths) const;
	void DrawFileDeletionConfirmation();
	void DrawFileReferenceImpact(const AssetReferenceImpact& impact, const char* countKey) const;
	void QueueFileDeletion(const vector<filesystem::path>& paths);
	void ProcessDeletionPreparation();
	void DeleteFiles(vector<FileMove> moves);
	filesystem::path FileHistoryRoot() const;
	filesystem::path CreateFileHistoryBatch() const;
	bool ReplayFileMoves(const vector<FileMove>& moves, bool forward);
	void SubmitFileMoves(vector<FileMove> moves, const char* label, bool execute);
	void FinishFileHistoryOperation();

	CommandHistory m_FileHistory;
	bool m_FileHistoryInvalid = false;
	vector<FileMove> m_PendingFileDeletion;
	AssetReferenceImpact m_DeletionImpact;
	std::future<DeletionPreparationResult> m_DeletionPreparation;
	std::jthread m_DeletionPreparationWorker;
	bool m_DeletionPreparationPending = false;
	bool m_DeletionPreparationCancelled = false;
	vector<FileMove> m_PendingFileMove;
	vector<filesystem::path> m_MoveSourcePaths;
	filesystem::path m_MoveDestination;
	BY_HANDLE_FILE_INFORMATION m_MoveDestinationIdentity{};
	bool m_MoveFromClipboard = false;
	bool m_PendingRename = false;
	AssetReferenceImpact m_MoveImpact;
	vector<AssetReferenceEdit> m_MoveReferences;
	bool m_MoveReferencesReady = false;
	bool m_MoveDocumentsAvailable = false;
	std::future<MovePreparationResult> m_MovePreparation;
	bool m_MovePreparationPending = false;
	bool m_MovePreparationCancelled = false;
	vector<filesystem::path> m_BackFolders;
	vector<filesystem::path> m_ForwardFolders;
	vector<filesystem::path> m_FileClipboardPaths;
	bool m_FileClipboardCut = false;
	vector<filesystem::path> m_DragPaths;
	filesystem::path m_PendingFileDropDirectory;
#pragma endregion


#pragma region TreeView
private:
	void DrawDirectoryTree(const filesystem::path& path, const filesystem::path& rootPath);
#pragma endregion

#pragma region GridView
private:
	void DrawGrid(bool enterPressed);
private:
	void BackButton();
	void RefreshButton();
	void DrawDirectoryGrid();
	void GridViewContextMenu(const filesystem::path& path);
	void AddressBar(float width);
private:
	bool SearchBar();
private:
	void ContentFilter();
private:
	struct GridSelectionItem
	{
		filesystem::path Path;
		ImGuiSelectionUserData Id = 0;
		size_t SourceIndex = 0;
		bool IsDirectory = false;
		bool IsReadOnly = false;
		std::string CacheKey;
		std::string ImGuiIdStr;
		ContentBrowserGridItem::AssetVisualDesc Visual;
		std::string TruncatedName;
		float TruncatedKey = -1.0f;
	};
	void DrawGridItem(const filesystem::directory_entry& directoryEntry, GridSelectionItem& item, ImDrawListSplitter* splitter = nullptr);
	vector<filesystem::path> GetInteractionPaths(ImGuiSelectionUserData itemId, const filesystem::path& path) const;
	void RebuildGridItems(const vector<filesystem::directory_entry>& source);
	void ApplyGridSelectionRequests(const ImGuiMultiSelectIO& selection);
private:
	void FileConfilictPopup();
#pragma endregion

#pragma region Helper Functions
private:
	void RefreshCacheIfNeeded();
	void OnExteranalDropped(const vector<string>& droppedFiles);
private:
	filesystem::path GenerateUniqueAssetPath(const filesystem::path& directory, const wstring& baseStem, const wstring& extension);
	void CreateEmptyMaterial(const filesystem::path& path);
	void CreateEmptyMaterialInstance(const filesystem::path& path);
	void CreateEmptyScene(const filesystem::path& path);
	void CreateEmptyPrefab(const filesystem::path& path);
	void CreateEmptyGraph(const filesystem::path& path);
	void CreateEmptyTimeline(const filesystem::path& path);
	void CreateEmptyParticleEffect(const filesystem::path& path);
#pragma endregion

#pragma region Workspace Settings
public:
	void CaptureState(EditorWorkspace& workspace) const override;
	void RestoreState(const EditorWorkspace& workspace) override;
#pragma endregion


#pragma region Member Variable
private:
	filesystem::path m_RootPath;
	filesystem::path m_EngineRootPath;
	filesystem::path m_EditorRootPath;
	filesystem::path m_CurrentDirectory;
	filesystem::path m_LastDirectory;
	bool m_NeedToExpandTree = false;
	bool m_ShowEngineContent = true;
private:
	filesystem::path m_RenameTargetPath;
	filesystem::path m_RenamingPath;
	filesystem::path m_QueuedRenamePath;
	std::optional<AssetPathMove> m_SubmittedRename;
	bool m_RenameTargetInTree = false;
	bool m_RenameInTree = false;
	bool m_RenameFocusRequested = false;
	vector<filesystem::path> m_InteractionPaths;
	vector<filesystem::path> m_ContextMenuPaths;
	wstring m_DragPayload;
	size_t m_DragAssetCount = 0;
	char m_RenameBuffer[256];
private:
	bool m_GridItemsDirty = true;
	bool m_GridUsesSearch = false;
	bool m_GridShowsEngineContent = true;
	uint32 m_GridFilterMask = 0;
	vector<GridSelectionItem> m_GridSelectionItems;
	std::unordered_map<ImGuiSelectionUserData, int> m_GridSelectionIndices;
	std::unordered_set<ImGuiSelectionUserData> m_SelectedGridIds;
	ImGuiSelectionUserData m_NextGridSelectionId = 0;
private:
	f32 m_ThumbnailSize = 64.0f;
	f32 m_Padding = 16.0f;
private:
	char m_SearchBuffer[256];
	uint32 m_FilterMask = 0;
	string m_LastSearchString;
	bool m_SearchAllFolders = false;
	vector<filesystem::directory_entry> m_SearchResults;
private:
	Engine::DelegateHandle m_AsyncCompletionHandle = 0;
	map<filesystem::path, vector<filesystem::path>> m_CachedSubdirectories;
	vector<filesystem::directory_entry> m_CachedEntries;
	filesystem::path m_LastWatchedDirectory;
	efsw::FileWatcher* m_FileWatcher = nullptr;
	efsw::WatchID m_WatchID = 0;
	efsw::WatchID m_EngineWatchID = 0;
	efsw::WatchID m_EditorWatchID = 0;
	class ContentBrowserFileListener* m_FileListener = nullptr;
	atomic<bool> m_NeedsCacheRefresh = { false };
private:
	bool m_OpenConflictPopup = false;
	bool m_ApplyToAllConflicts = false;
	vector<FileDropConflict> m_PendingConflicts;
private:
	// 모델 임포트 옵션 모달 상태
	bool m_OpenModelImportModal = false;
	filesystem::path m_PendingModelImportPath;
	filesystem::path m_PendingModelDestDir;
	ModelImportDesc m_CurrentModelImportDesc;
	// 모델 임포트 옵션 모달 및 3D 프리뷰어
	bool m_IsImportModalActive = false;
	PreviewViewportWidget m_ImportPreviewWidget;
	Engine::Entity* m_ImportPreviewEntity = nullptr;
private:
	filesystem::path m_RevealedAssetPath;
	bool m_RevealScrollPending = false;
private:
	// 폴더 트리 노드별 읽기 전용 여부 캐시 (경로 정규화 비용을 프레임마다 반복하지 않기 위함)
	std::unordered_map<std::wstring, bool> m_ReadOnlyCache;
#pragma endregion
};
END

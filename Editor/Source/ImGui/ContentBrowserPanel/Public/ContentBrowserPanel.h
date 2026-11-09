#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include <efsw/efsw.hpp>
#include <unordered_map>
#include <unordered_set>
#include "ModelImporter.h"
#include "PreviewViewportWidget.h"

BEGIN(Editor)

class ContentBrowserFileListener : public efsw::FileWatchListener
{
public:
	std::atomic<bool>* m_RefreshFlag;
	ContentBrowserFileListener(std::atomic<bool>* flag) : m_RefreshFlag(flag) {}
	void handleFileAction(efsw::WatchID watchid, const std::string& dir, const std::string& filename, efsw::Action action, std::string oldFilename) override;
};

struct FileDropConflict
{
	filesystem::path Source;
	filesystem::path Dest;
};

class ContentBrowserPanel : public IImGui
{
public:
	ContentBrowserPanel() = default;
	virtual ~ContentBrowserPanel() = default;
	virtual void Update(f32 dt) override;
	virtual void Draw() override;
	virtual void ProcessEvent(const SDL_Event* event) override;
	virtual void Free() override;
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::ContentBrowser; }
	void SetRenameTarget(const filesystem::path& path) { m_RenameTargetPath = path; }
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
	void DrawGridItem(const filesystem::directory_entry& directoryEntry, ImGuiSelectionUserData itemId);
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
	vector<filesystem::path> m_InteractionPaths;
	char m_RenameBuffer[256];
private:
	struct GridSelectionItem
	{
		filesystem::path Path;
		ImGuiSelectionUserData Id;
		size_t SourceIndex;
	};
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
#pragma endregion
};
END

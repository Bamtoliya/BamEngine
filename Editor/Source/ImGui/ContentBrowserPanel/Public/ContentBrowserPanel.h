#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include <filesystem>
#include <unordered_map>
#include <efsw/efsw.hpp>

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

class ContentBrowserPanel : public ImGuiInterface
{
public:
	ContentBrowserPanel() = default;
	virtual ~ContentBrowserPanel() = default;
	virtual EResult Initialize(void* arg = nullptr);
	virtual void Draw() override;
	virtual void ProcessEvent(const SDL_Event* event) override;
	virtual void Free() override;

#pragma region Request
public:
	void RequestRefresh() { m_NeedsCacheRefresh = true; }
#pragma endregion



#pragma region TreeView
private:
	void DrawDirectoryTree(const filesystem::path& path);
	void TreeViewContextMenu(const filesystem::path& path);
#pragma endregion

#pragma region GridView
private:
	void DrawGrid(bool enterPressed);
private:
	void BackButton();
	void RefreshButton();
	void DrawDirectoryGrid();
	void GridViewContextMenu(const filesystem::path& path);
	void AddressBar();
private:
	bool SearchBar();
private:
	void ContentFilter();
private:
	void DrawGridItem(const filesystem::directory_entry& directoryEntry);
private:
	void FileConfilictPopup();
#pragma endregion
#pragma region Helper Functions
private:
	void OnExteranalDropped(const vector<string>& droppedFiles);
private:
	void DrawRename(const filesystem::path& path);
private:
	void CreateEmptyMaterial(const filesystem::path& path);
#pragma endregion

	

#pragma region Member Variable
private:
	filesystem::path m_RootPath;
	filesystem::path m_CurrentDirectory;
	filesystem::path m_LastDirectory;
	bool m_NeedToExpandTree = false;
private:
	filesystem::path m_RenamingPath;
	char m_RenameBuffer[256];
private:
	f32 m_ThumbnailSize = 64.0f;
	f32 m_Padding = 16.0f;
private:
	char m_SearchBuffer[256];
	uint32 m_FilterMask = 0;
	string m_LastSearchString;
	vector<filesystem::directory_entry> m_SearchResults;

private:
	vector<filesystem::directory_entry> m_CachedEntries;
	filesystem::path m_LastWatchedDirectory;
	efsw::FileWatcher* m_FileWatcher;
	efsw::WatchID m_WatchID;
	class ContentBrowserFileListener* m_FileListener;
	atomic<bool> m_NeedsCacheRefresh = { false };

private:
	bool m_OpenConflictPopup = false;
	bool m_ApplyToAllConflicts = false;
	vector< FileDropConflict> m_PendingConflicts;

private:
	//inline static unordered_map<string, void*> m_ThumbnailCache;
	//inline static unordered_map<string, RHITexture*> m_ThumbnailTextures;
#pragma endregion
};
END
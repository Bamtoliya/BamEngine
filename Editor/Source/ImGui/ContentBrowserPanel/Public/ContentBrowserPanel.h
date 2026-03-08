#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include <filesystem>
#include <unordered_map>

BEGIN(Editor)
class ContentBrowserPanel : public ImGuiInterface
{
public:
	ContentBrowserPanel() = default;
	virtual ~ContentBrowserPanel() = default;
	virtual EResult Initialize(void* arg = nullptr);
	virtual void Draw() override;
	virtual void Free() override;



#pragma region TreeView
private:
	void DrawDirectoryTree(const filesystem::path& path);
	void TreeViewContextMenu(const filesystem::path& path);
#pragma endregion

#pragma region GridView
private:
	void BackButton();
	void DrawDirectoryGrid();
	void GridViewContextMenu(const filesystem::path& path);
	void AddressBar();
private:
	void DrawThumbnail(const filesystem::directory_entry directoryEntry);
	void GridItemContextMenu(const filesystem::path& path);
	void DragAndDropTarget(const filesystem::path& path);
#pragma endregion
#pragma region Helper Functions
private:
	void DrawRename(const filesystem::path& path);
	void* LoadThumbnail(const filesystem::path& path);
#pragma endregion

	

#pragma region Member Variable
private:
	filesystem::path m_RootPath;
	filesystem::path m_CurrentDirectory;
	filesystem::path m_LastDirectory;
	bool m_NeedToExpandTree = false;

	filesystem::path m_RenamingPath;
	char m_RenameBuffer[256];

	f32 m_ThumbnailSize = 64.0f;
	f32 m_Padding = 16.0f;
private:
	inline static unordered_map<string, void*> m_ThumbnailCache;
	inline static unordered_map<string, RHITexture*> m_ThumbnailTextures;
#pragma endregion
};
END
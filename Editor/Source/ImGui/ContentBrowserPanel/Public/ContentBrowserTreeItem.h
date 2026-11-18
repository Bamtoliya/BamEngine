#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class ContentBrowserPanel;
class ContentBrowserTreeItem
{
public:
	static bool DrawNode(ContentBrowserPanel& browser, const std::filesystem::path& path,
		const std::filesystem::path& rootPath, bool hasChildDirectory, bool isSelected, bool needToExpand,
		bool isReadOnly, bool isEngineRoot, bool isEditorRoot);

private:
	static void ContextMenu(ContentBrowserPanel& browser, const std::filesystem::path& path,
		const std::filesystem::path& rootPath);

	static void DrawRename(ContentBrowserPanel& browser, const std::filesystem::path& path);
};
END
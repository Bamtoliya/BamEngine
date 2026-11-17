#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class ContentBrowserPanel;
class ContentBrowserTreeItem
{
public:
	static bool DrawNode(
		ContentBrowserPanel& browser,
		const std::filesystem::path& path,
		const std::filesystem::path& rootPath,
		bool hasChildDirectory,
		bool isSelected,
		bool needToExpand,
		bool isReadOnly,
		bool isEngineRoot,
		bool isEditorRoot,
		std::filesystem::path& outRenamingPath,
		char* renameBuffer,
		size_t renameBufferSize,
		std::filesystem::path& currentDirectory
	);

private:
	static void ContextMenu(ContentBrowserPanel& browser, const std::filesystem::path& path,
		const std::filesystem::path& rootPath);

	static void DrawRename(
		const std::filesystem::path& path,
		std::filesystem::path& outRenamingPath,
		char* renameBuffer,
		size_t renameBufferSize,
		std::filesystem::path& currentDirectory
	);
};
END
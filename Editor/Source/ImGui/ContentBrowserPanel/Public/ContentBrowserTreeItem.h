#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class ContentBrowserTreeItem
{
public:
	static bool DrawNode(
		const std::filesystem::path& path,
		const std::filesystem::path& rootPath,
		bool hasChildDirectory,
		bool isSelected,
		bool needToExpand,
		std::filesystem::path& outRenamingPath,
		char* renameBuffer,
		size_t renameBufferSize,
		std::filesystem::path& currentDirectory
	);

private:
	static void ContextMenu(
		const std::filesystem::path& path,
		const std::filesystem::path& rootPath,
		std::filesystem::path& outRenamingPath,
		char* renameBuffer,
		size_t renameBufferSize,
		std::filesystem::path& currentDirectory
	);

	static void DrawRename(
		const std::filesystem::path& path,
		std::filesystem::path& outRenamingPath,
		char* renameBuffer,
		size_t renameBufferSize,
		std::filesystem::path& currentDirectory
	);
};
END
#pragma once

#include "Editor_Includes.h"
#include <filesystem>

BEGIN(Editor)
class ContentBrowserGridItem
{
public:
	static void Draw(const std::filesystem::directory_entry& directoryEntry,
		const std::filesystem::path& rootPath,
		void* thumbnailTexID,
		f32 thumbnailSize,
		f32 padding,
		std::filesystem::path& outRenamingPath,
		char* renameBuffer,
		size_t renameBufferSize,
		std::filesystem::path& currentDirectory,
		char* searchBuffer);

private:
	static void Thumbnail(const std::filesystem::directory_entry& directoryEntry, void* thumbnailTexID, f32 thumbnailSize, f32 padding);
	static void ContextMenu(const std::filesystem::path& path, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize);
	static void ToolTip(const std::filesystem::directory_entry& directoryEntry);
	static void DragAndDropTarget(const std::filesystem::path& relativePath);

	// 이름 변경 로직과 네임 박스 렌더링 로직을 분리
	static void DrawRename(const std::filesystem::path& path, f32 thumbnailSize, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory);
	static void DrawNameBox(const std::filesystem::directory_entry& directoryEntry, const std::filesystem::path& rootPath, f32 thumbnailSize, std::filesystem::path& currentDirectory, char* searchBuffer);
};
END
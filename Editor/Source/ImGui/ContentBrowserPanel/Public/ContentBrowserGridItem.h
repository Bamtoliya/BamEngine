#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class ContentBrowserGridItem
{
public:
	static bool Draw(const std::filesystem::directory_entry& directoryEntry, const std::filesystem::path& rootPath,
		ImTextureID thumbnailTexID, f32 thumbnailSize, ImGuiSelectionUserData itemId, bool selected,
		std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize,
		std::filesystem::path& currentDirectory, char* searchBuffer);
	static float GetNameBoxHeight();

private:
	static void DrawThumbnail(const std::filesystem::directory_entry& directoryEntry, ImTextureID thumbnailTexID,
		float thumbnailSize, const ImVec2& tileMin);
	static void ContextMenu(const std::filesystem::path& path, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory, char* searchBuffer);
	static void ToolTip(const std::filesystem::directory_entry& directoryEntry);
	static bool DragAndDropTarget(const std::filesystem::path& relativePath);

	// 이름 변경 로직과 네임 박스 렌더링 로직을 분리
	static void DrawRename(const std::filesystem::path& path, f32 thumbnailSize, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory);
	static void DrawNameBox(const std::filesystem::directory_entry& directoryEntry, const ImVec2& boxMin,
		float boxWidth, float boxHeight);

private:
	static void ContextMenu_Material(const std::filesystem::path& path);
	static void ContextMenu_Model(const std::filesystem::path& path);
	static void ContextMenu_Texture(const std::filesystem::path& path);
	static void ContextMenu_Sprite(const std::filesystem::path& path);
	static void ContextMenu_Shader(const std::filesystem::path& path);
	static bool ConvertJsonToBinary(const std::filesystem::path& path);
	static bool ConvertBinaryToJson(const std::filesystem::path& path);
};
END
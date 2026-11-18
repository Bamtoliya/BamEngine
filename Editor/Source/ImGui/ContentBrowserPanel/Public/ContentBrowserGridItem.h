#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class ContentBrowserPanel;

class ContentBrowserGridItem
{
public:
	static bool Draw(ContentBrowserPanel& browser, const std::filesystem::directory_entry& directoryEntry,
		ImTextureID thumbnailTexID, Engine::Handle thumbnailSpriteHandle, f32 thumbnailSize,
		ImGuiSelectionUserData itemId, bool selected);
	static float GetNameBoxHeight();

private:
	static void DrawThumbnail(const std::filesystem::directory_entry& directoryEntry, ImTextureID thumbnailTexID, Engine::Handle thumbnailSpriteHandle,
		float thumbnailSize, const ImVec2& tileMin);
	static void ContextMenu(ContentBrowserPanel& browser, ImGuiSelectionUserData itemId, const std::filesystem::path& path,
		std::filesystem::path& currentDirectory);
	static void ToolTip(const std::filesystem::directory_entry& directoryEntry);
	static bool DrawDragSource(ContentBrowserPanel& browser, ImGuiSelectionUserData itemId, const std::filesystem::path& path);

	// 이름 변경 로직과 네임 박스 렌더링 로직을 분리
	static void DrawRename(ContentBrowserPanel& browser, const std::filesystem::path& path, f32 thumbnailSize);
	static void DrawNameBox(const std::filesystem::directory_entry& directoryEntry, const ImVec2& boxMin,
		float boxWidth, float boxHeight);

private:
	static void ContextMenu_Material(ContentBrowserPanel& browser, const std::filesystem::path& path);
	static void ContextMenu_Model(const std::filesystem::path& path);
	static void ContextMenu_Texture(ContentBrowserPanel& browser, const std::filesystem::path& path);
	static void ContextMenu_Sprite(const std::filesystem::path& path);
	static void ContextMenu_Shader(const std::filesystem::path& path);
	static bool ConvertJsonToBinary(const std::filesystem::path& path);
	static bool ConvertBinaryToJson(const std::filesystem::path& path);
};
END

#pragma once

#include "Editor_Includes.h"
#include <string>

BEGIN(Editor)
class ContentBrowserPanel;

class ContentBrowserGridItem
{
public:
	struct AssetVisualDesc
	{
		const char* Icon = nullptr;
		ImVec4 BgColor = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
		ImVec4 AccentColor = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
		std::string BadgeText;
		std::string CleanName;
	};

	struct TileRenderContext
	{
		ImTextureID ThumbnailTexID{};
		ImVec2 UV0{ 0.0f, 0.0f };
		ImVec2 UV1{ 1.0f, 1.0f };
		Engine::Handle SpriteHandle{};
		const AssetVisualDesc* Visual = nullptr;
		bool IsReadOnly = false;
		const char* ImGuiId = nullptr;
		std::string* CachedTruncatedName = nullptr;
		float* CachedTruncatedKey = nullptr;
		ImDrawListSplitter* Splitter = nullptr;
	};

public:
	static AssetVisualDesc DescribeAssetVisual(const std::filesystem::path& path, bool isDirectory);
	static bool Draw(ContentBrowserPanel& browser, const std::filesystem::directory_entry& directoryEntry,
		const TileRenderContext& ctx, f32 thumbnailSize, ImGuiSelectionUserData itemId, bool selected);
	static bool Draw(ContentBrowserPanel& browser, const std::filesystem::directory_entry& directoryEntry,
		ImTextureID thumbnailTexID, Engine::Handle thumbnailSpriteHandle, f32 thumbnailSize,
		ImGuiSelectionUserData itemId, bool selected);
	static float GetNameBoxHeight();

private:
	static void DrawThumbnail(const std::filesystem::directory_entry& directoryEntry, const TileRenderContext& ctx,
		float thumbnailSize, const ImVec2& tileMin);
	static void ContextMenu(ContentBrowserPanel& browser, ImGuiSelectionUserData itemId, const std::filesystem::path& path,
		std::filesystem::path& currentDirectory);
	static void ToolTip(const std::filesystem::directory_entry& directoryEntry, const TileRenderContext& ctx);
	static bool DrawDragSource(ContentBrowserPanel& browser, ImGuiSelectionUserData itemId, const std::filesystem::path& path);

	// 이름 변경 로직과 네임 박스 렌더링 로직을 분리
	static void DrawRename(ContentBrowserPanel& browser, const std::filesystem::path& path, f32 thumbnailSize);
	static void DrawNameBox(const std::filesystem::directory_entry& directoryEntry, const TileRenderContext& ctx,
		const ImVec2& boxMin, float boxWidth, float boxHeight);

private:
	static void ContextMenu_Material(ContentBrowserPanel& browser, const std::filesystem::path& path);
	static void ContextMenu_Model(ContentBrowserPanel& browser, const std::filesystem::path& path);
	static void ContextMenu_Texture(ContentBrowserPanel& browser, const std::filesystem::path& path);
	static void ContextMenu_Sprite(const std::filesystem::path& path);
	static void ContextMenu_Shader(const std::filesystem::path& path);
	static bool ConvertJsonToBinary(const std::filesystem::path& path);
	static bool ConvertBinaryToJson(const std::filesystem::path& path);
};
END

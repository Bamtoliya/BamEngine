#pragma once

#include "Editor_Includes.h"
#include "LogEntry.h"
#include <imgui.h>
#include <cstddef>
#include <vector>

#define MAX_CONSOLE_ITEMS 1000

BEGIN(Editor)
class ConsolePanel
{
public:
	void Draw(bool isRightMouseButtonDown);
	void Update(float dt);
	std::vector<std::byte> CapturePluginState() const;
	bool RestorePluginState(const void* state, std::size_t stateSize);
	float GetDetailViewHeight() const { return m_DetailViewHeight; }
	void SetDetailViewHeight(float height) { m_DetailViewHeight = height; }
private:
	struct ConsoleItem
	{
		LogEntry Entry;
		std::string DisplayText;
		std::string SingleLineMessage;
		std::string TimeString;
	};
	struct ConsoleRow
	{
		std::size_t ItemIndex = 0;
		std::uint32_t RepeatCount = 1;
		bool CountCapped = false;
	};

	static constexpr std::uint32_t MaxRepeatCount = 999;
	static_assert(MaxRepeatCount > 0);

	void ClearLog();

	void DrawOptions();
	void DrawLevelFilter();
	void DrawSourceFilter();

	void DrawSearch();
	void DrawStatus();

	static std::string FormatTime(const std::chrono::system_clock::time_point& timestamp);
	static void DrawLevelBadge(Engine::ELogLevel level);
	static void DrawCategoryBadge(std::string_view category);
	static void DrawRepeatBadge(std::uint32_t count, bool capped);

	void DrawLog();
	void DrawLogItem(const ConsoleRow& row, float rowHeight);
	void RebuildVisibleRows();
	void OnLanguageChanged(ELocalizationLanguage newLang);

private:
	static std::string BuildDisplayText(const LogEntry& entry);
	static const char* LevelName(Engine::ELogLevel level);
	static const char* LevelIcon(Engine::ELogLevel level);
	static const char* CategoryIcon(std::string_view category);
	static ImVec4 LevelColor(Engine::ELogLevel level);
private:
	deque<ConsoleItem> m_Items;
	ImGuiTextFilter m_Filter;
	std::vector<ConsoleRow> m_VisibleRows;
	bool m_CollapseDuplicates = true;
	bool m_AutoScroll = true;
	bool m_ScrollToBottom = false;
	bool m_HasNewEntries = false;
	bool m_IsRightMouseButtonDown = false;
	bool m_LanguageInitialized = false;
	ELocalizationLanguage m_CurrentLanguage = ELocalizationLanguage::English;

	array<bool, 5> m_ShowLevels{ true, true, true, true, true };
	uint64 m_DroppedCount = 0;
	uint64 m_EvictedCount = 0;

	std::optional<std::string> m_SourceFilter;
	int m_SelectedRowIndex = -1;
	float m_DetailViewHeight = 160.0f;
};
END
#pragma once

#include "ImGuiInterface.h"
#include "LogEntry.h"

#define MAX_CONSOLE_ITEMS 1000

BEGIN(Editor)
class ConsolePanel : public IImGui
{
public:
	ConsolePanel() { m_Name = L"Console"; m_DisplayNameKey = "UI_CONSOLE"; };
	virtual ~ConsolePanel() = default;
	virtual void Draw() override;
	virtual void Update(f32 dt) override;
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

private:
	static std::string BuildDisplayText(const LogEntry& entry);
	static const char* LevelName(Engine::ELogLevel level);
	static const char* LevelIcon(Engine::ELogLevel level);
	static const char* CategoryIcon(std::string_view category);
	static ImVec4 LevelColor(Engine::ELogLevel level);
private:
private:
	deque<ConsoleItem> m_Items;
	ImGuiTextFilter m_Filter;
	std::vector<ConsoleRow> m_VisibleRows;
	bool m_CollapseDuplicates = true;
	bool m_AutoScroll = true;
	bool m_ScrollToBottom = false;
	bool m_HasNewEntries = false;

	array<bool, 5> m_ShowLevels{ true, true, true, true, true };
	uint64 m_DroppedCount = 0;
	uint64 m_EvictedCount = 0;

	std::optional<std::string> m_SourceFilter;
	int m_SelectedRowIndex = -1;
	float m_DetailViewHeight = 160.0f;
};
END
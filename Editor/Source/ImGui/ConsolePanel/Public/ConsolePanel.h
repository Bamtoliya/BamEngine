#pragma once

#include "ImGuiInterface.h"
#include "LogEntry.h"
#include <optional>

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

	void DrawLog();
	void DrawLogItem(const ConsoleRow& row);
	void RebuildVisibleRows();

private:
	static std::string BuildDisplayText(const LogEntry& entry);
	static const char* LevelName(Engine::ELogLevel level);
	static ImVec4 LevelColor(Engine::ELogLevel level);
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
};
END
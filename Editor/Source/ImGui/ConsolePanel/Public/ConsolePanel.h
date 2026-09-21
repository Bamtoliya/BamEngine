#pragma once

#include "ImGuiInterface.h"
#include "LogEntry.h"

#define MAX_CONSOLE_ITEMS 1000

BEGIN(Editor)
class ConsolePanel : public ImGuiInterface
{
public:
	ConsolePanel() { m_Name = L"Console"; };
	virtual ~ConsolePanel() = default;
	virtual void Draw() override;
	virtual void Update(f32 dt) override;
private:
	struct ConsoleItem
	{
		LogEntry Entry;
		std::string DisplayText;
	};
	void ClearLog();

	void DrawOptions();
	void DrawLevelFilter();

	void DrawSearch();
	void DrawStatus();

	void DrawLog();
	void DrawLogItem(const ConsoleItem& item);
	
	void RebuildVisibleIndices();

private:
	static std::string BuildDisplayText(const LogEntry& entry);
	static const char* LevelName(Engine::ELogLevel level);
	static ImVec4 LevelColor(Engine::ELogLevel level);
private:
	deque<ConsoleItem> m_Items;
	ImGuiTextFilter m_Filter;
	std::vector<int> m_VisibleIndices;
	bool m_AutoScroll = true;
	bool m_ScrollToBottom = false;
	bool m_HasNewEntries = false;

	array<bool, 5> m_ShowLevels{ true, true, true, true, true };
	uint64 m_DroppedCount = 0;
	uint64 m_EvictedCount = 0;


};
END
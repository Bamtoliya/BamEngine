// 파일: Editor/Source/ImGui/HistoryPanel/Private/HistoryPanel.cpp

#include "HistoryPanel.h"
#include "EditorUI.h"
#include "Application.h"
#include "SceneEditHistory.h"
#include "NotificationManager.h"
#include "LocalizationManager.h"
#include "ShortcutManager.h"
#include "IconsFontAwesome7.h"

HistoryPanel::HistoryPanel()
{
	m_Name = L"HistoryPanel";
	m_DisplayNameKey = "History.Title";
	m_Open = false;
}

void HistoryPanel::Draw()
{
	if (!m_Open) return;

	auto& history = Application::Get().GetEditHistory();
	auto& commands = history.Commands();

	const bool hasUnsaved = history.HasUnsavedChanges();
	const ImGuiWindowFlags flags = hasUnsaved ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None;

	const std::string windowTitle = GetDisplayName() + "###HistoryPanel";
	if (BeginPanelWindow(windowTitle.c_str(), m_Open, flags))
	{
		EditorUI::ToolbarLayout toolbar;
		auto& shortcuts = ShortcutManager::Get();
		EditorUI::IconButtonOptions options;
		options.Enabled = history.CanUndo();
		options.ShortcutHint = shortcuts.GetShortcutHint("Edit.Undo");
		if (toolbar.Icon("HistoryUndo", ICON_FA_ROTATE_LEFT, "EditorCommon.Undo", options)) history.Undo();
		options.Enabled = history.CanRedo();
		options.ShortcutHint = shortcuts.GetShortcutHint("Edit.Redo");
		if (toolbar.Icon("HistoryRedo", ICON_FA_ROTATE_RIGHT, "EditorCommon.Redo", options)) history.Redo();
		if (toolbar.Button("HistoryClear", ICON_FA_TRASH_CAN, "History.Clear"))
		{
			history.Clear();
			NotificationManager::Get().AddNotification(LOCAL("History.Title"), LOCAL("History.Cleared"), ELogLevel::Info);
		}
		ImGui::PushStyleColor(ImGuiCol_Text, hasUnsaved ? ImVec4(1.0f, 0.6f, 0.2f, 1.0f) : ImVec4(0.3f, 1.0f, 0.4f, 1.0f));
		toolbar.Text(LOCAL_CSTR(hasUnsaved ? "History.UnsavedChanges" : "History.AllChangesSaved"), false);
		ImGui::PopStyleColor();

		ImGui::Separator();

		// ── 2. 명령 스택 리스트뷰 ──
		const auto historyItems = commands.GetHistoryView();
		const std::size_t cursor = commands.GetCursor();
		const uint64 savedStateId = history.GetSavedStateId();

		if (ImGui::BeginTable("HistoryTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
		{
			const std::string colStatus = LOCAL("History.Status") + "##Status";
			const std::string colAction = LOCAL("History.CommandAction") + "##Action";
			const std::string colSaved = LOCAL("History.SavedColumn") + "##Saved";

			// [컬럼 너비 최적화]: 상태(50px) 및 저장(45px) 열을 슬림화하여 작업 내용 열에 공간 집중
			ImGui::TableSetupColumn(colStatus.c_str(), ImGuiTableColumnFlags_WidthFixed, 50.0f);
			ImGui::TableSetupColumn(colAction.c_str(), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(colSaved.c_str(), ImGuiTableColumnFlags_WidthFixed, 45.0f);
			ImGui::TableHeadersRow();

			// 1) 초기 상태 (Initial State) - 커서 0
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			if (cursor == 0)
				ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("History.Current"));

			ImGui::TableSetColumnIndex(1);
			const bool initialSelected = (cursor == 0);
			if (ImGui::Selectable(LOCAL_LABEL("History.InitialState").c_str(), initialSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				commands.UndoTo(0);
			}

			ImGui::TableSetColumnIndex(2);
			ImGui::AlignTextToFramePadding();
			if (savedStateId == 0)
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", LOCAL_CSTR("History.SavedMarker"));

			// 2) 실행된 명령 스택 순회
			for (std::size_t i = 0; i < historyItems.size(); ++i)
			{
				const auto& item = historyItems[i];
				const std::size_t itemCursor = i + 1;
				const bool isCurrent = (cursor == itemCursor);
				const bool isPast = (itemCursor <= cursor);
				const bool isSaved = (savedStateId != 0 && item.StateId == savedStateId);

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				if (isCurrent)
					ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("History.Current"));

				ImGui::TableSetColumnIndex(1);
				std::string localizedName = LOCAL(item.Label == "Unnamed" ? "History.Unnamed" : item.Label);
				if (localizedName.empty()) localizedName = item.Label;

				char rowLabel[256];
				snprintf(rowLabel, sizeof(rowLabel), "%zu. %s##%zu", i + 1, localizedName.c_str(), i);

				if (!isPast)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));

				if (ImGui::Selectable(rowLabel, isCurrent, ImGuiSelectableFlags_SpanAllColumns))
				{
					commands.UndoTo(itemCursor);
				}

				// [호버 툴팁]: 긴 명령 텍스트가 잘려도 전체 이름 확인 가능
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%zu. %s", i + 1, localizedName.c_str());
				}

				if (!isPast)
					ImGui::PopStyleColor();

				ImGui::TableSetColumnIndex(2);
				ImGui::AlignTextToFramePadding();
				if (isSaved)
					ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", LOCAL_CSTR("History.SavedMarker"));
			}

			ImGui::EndTable();
		}
	}
	ImGui::End();
}
#include "HistoryPanel.h"
#include "Application.h"
#include "SceneEditHistory.h"
#include "NotificationManager.h"
#include "LocalizationManager.h"

HistoryPanel::HistoryPanel()
{
	m_Name = L"HistoryPanel";
	m_DisplayNameKey = "History.Title";
	m_Open = false; // 기본값은 닫힘 상태
}

EResult HistoryPanel::Initialize(void* arg)
{
	(void)arg;
	return EResult::Success;
}

void HistoryPanel::Draw()
{
	if (!m_Open) return;

	auto& history = Application::Get().GetEditHistory();
	auto& commands = history.Commands();

	const bool hasUnsaved = history.HasUnsavedChanges();
	const ImGuiWindowFlags flags = hasUnsaved ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None;

	const std::string windowTitle = GetDisplayName() + "###HistoryPanel";
	if (ImGui::Begin(windowTitle.c_str(), &m_Open, flags))
	{
		// ── 1. 상단 컨트롤 툴바 ──
		ImGui::BeginDisabled(!history.CanUndo());
		const std::string undoLabel = LOCAL_FMT("History.ActionShortcut", LOCAL("UI_UNDO"), "Ctrl+Z") + "###HistoryUndo";
		if (ImGui::Button(undoLabel.c_str()))
			history.Undo();
		ImGui::EndDisabled();

		ImGui::SameLine();

		ImGui::BeginDisabled(!history.CanRedo());
		const std::string redoLabel = LOCAL_FMT("History.ActionShortcut", LOCAL("UI_REDO"), "Ctrl+Y") + "###HistoryRedo";
		if (ImGui::Button(redoLabel.c_str()))
			history.Redo();
		ImGui::EndDisabled();

		ImGui::SameLine();

		if (ImGui::Button(LOCAL_LABEL("History.Clear").c_str()))
		{
			history.Clear();
			NotificationManager::Get().AddNotification(LOCAL("History.Title"), LOCAL("History.Cleared"), ELogLevel::Info);
		}

		ImGui::SameLine();
		if (hasUnsaved)
			ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", LOCAL_CSTR("History.UnsavedChanges"));
		else
			ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", LOCAL_CSTR("History.AllChangesSaved"));

		ImGui::Separator();

		// ── 2. 명령 스택 리스트뷰 ──
		const auto historyItems = commands.GetHistoryView();
		const std::size_t cursor = commands.GetCursor();
		const uint64 savedStateId = history.GetSavedStateId();

		if (ImGui::BeginTable("HistoryTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
		{
			ImGui::TableSetupColumn(LOCAL_LABEL("History.Status").c_str(), ImGuiTableColumnFlags_WidthFixed, 75.0f);
			ImGui::TableSetupColumn(LOCAL_LABEL("History.CommandAction").c_str(), ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn(LOCAL_LABEL("History.SavedColumn").c_str(), ImGuiTableColumnFlags_WidthFixed, 60.0f);
			ImGui::TableHeadersRow();

			// 1) 초기 상태 (Initial State) - 커서 0
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			if (cursor == 0)
				ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("History.Current"));

			ImGui::TableSetColumnIndex(1);
			const bool initialSelected = (cursor == 0);
			if (ImGui::Selectable(LOCAL_LABEL("History.InitialState").c_str(), initialSelected, ImGuiSelectableFlags_SpanAllColumns))
			{
				commands.UndoTo(0);
			}

			ImGui::TableSetColumnIndex(2);
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
				if (isCurrent)
					ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("History.Current"));

				ImGui::TableSetColumnIndex(1);
				std::string localizedName = LOCAL(item.Label == "Unnamed" ? "History.Unnamed" : item.Label);
				if (localizedName.empty()) localizedName = item.Label;

				char rowLabel[256];
				snprintf(rowLabel, sizeof(rowLabel), "%zu. %s##%zu", i + 1, localizedName.c_str(), i);

				// 과거에 실행된 명령은 일반 텍스트, 아직 실행되지 않은 Redo 대기 명령은 어둡게 표시
				if (!isPast)
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));

				if (ImGui::Selectable(rowLabel, isCurrent, ImGuiSelectableFlags_SpanAllColumns))
				{
					commands.UndoTo(itemCursor);
				}

				if (!isPast)
					ImGui::PopStyleColor();

				ImGui::TableSetColumnIndex(2);
				if (isSaved)
					ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", LOCAL_CSTR("History.SavedMarker"));
			}

			ImGui::EndTable();
		}
	}
	ImGui::End();
}

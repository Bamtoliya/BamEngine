// 파일: Editor/Source/ImGui/HistoryPanel/Private/HistoryPanel.cpp

#include "HistoryPanel.h"
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
		const float availWidth = ImGui::GetContentRegionAvail().x;
		constexpr float kCompactBreakpoint = 360.0f;
		const bool isCompact = (availWidth < kCompactBreakpoint);
		const float frameHeight = ImGui::GetFrameHeight();

		// ShortcutManager에서 최신 동적 단축키 힌트 조회
		auto& sm = ShortcutManager::Get();
		const char* undoHint = sm.GetShortcutHint("Edit.Undo");
		const char* redoHint = sm.GetShortcutHint("Edit.Redo");

		// ── 1. 상단 컨트롤 툴바 (반응형 + 동적 숏컷 힌트) ──
		// [Undo 버튼]
		ImGui::BeginDisabled(!history.CanUndo());
		if (isCompact)
		{
			if (ImGui::Button((ICON_FA_ROTATE_LEFT "###HistoryUndo"), ImVec2(frameHeight, frameHeight)))
				history.Undo();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				if (undoHint && undoHint[0] != '\0')
					ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("EditorCommon.Undo"), undoHint);
				else
					ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.Undo"));
			}
		}
		else
		{
			const std::string undoText = (undoHint && undoHint[0] != '\0') ?
				LOCAL_FMT("History.ActionShortcut", LOCAL("EditorCommon.Undo"), undoHint) : LOCAL("EditorCommon.Undo");
			const std::string undoLabel = undoText + "###HistoryUndo";
			if (ImGui::Button(undoLabel.c_str()))
				history.Undo();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();

		// [Redo 버튼]
		ImGui::BeginDisabled(!history.CanRedo());
		if (isCompact)
		{
			if (ImGui::Button((ICON_FA_ROTATE_RIGHT "###HistoryRedo"), ImVec2(frameHeight, frameHeight)))
				history.Redo();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			{
				if (redoHint && redoHint[0] != '\0')
					ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("EditorCommon.Redo"), redoHint);
				else
					ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.Redo"));
			}
		}
		else
		{
			const std::string redoText = (redoHint && redoHint[0] != '\0') ?
				LOCAL_FMT("History.ActionShortcut", LOCAL("EditorCommon.Redo"), redoHint) : LOCAL("EditorCommon.Redo");
			const std::string redoLabel = redoText + "###HistoryRedo";
			if (ImGui::Button(redoLabel.c_str()))
				history.Redo();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();

		// [Clear 버튼]
		if (isCompact)
		{
			if (ImGui::Button((ICON_FA_TRASH_CAN "###HistoryClear"), ImVec2(frameHeight, frameHeight)))
			{
				history.Clear();
				NotificationManager::Get().AddNotification(LOCAL("History.Title"), LOCAL("History.Cleared"), ELogLevel::Info);
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", LOCAL_CSTR("History.Clear"));
		}
		else
		{
			if (ImGui::Button(LOCAL_LABEL("History.Clear").c_str()))
			{
				history.Clear();
				NotificationManager::Get().AddNotification(LOCAL("History.Title"), LOCAL("History.Cleared"), ELogLevel::Info);
			}
		}

		ImGui::SameLine();

		// [저장 상태 표시]
		ImGui::AlignTextToFramePadding();
		if (isCompact)
		{
			if (hasUnsaved)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), ICON_FA_FLOPPY_DISK " *");
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", LOCAL_CSTR("History.UnsavedChanges"));
			}
			else
			{
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), ICON_FA_FLOPPY_DISK);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", LOCAL_CSTR("History.AllChangesSaved"));
			}
		}
		else
		{
			if (hasUnsaved)
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", LOCAL_CSTR("History.UnsavedChanges"));
			else
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.4f, 1.0f), "%s", LOCAL_CSTR("History.AllChangesSaved"));
		}

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
#include "ShortcutSettingsWidget.h"
#include "Editor_Includes.h"
#include "InputManager.h"
#include "LocalizationManager.h"
#include "ShortcutManager.h"

void ShortcutSettingsWidget::Draw(const ImGuiTextFilter& searchFilter)
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryShortcuts"));
    ImGui::Separator();
    ImGui::Spacing();

    HandleCapture();

    if (ImGui::Button(LOCAL_CSTR("Shortcuts.ResetAll")))
    {
        ShortcutManager::Get().ResetAllBindings();
        ShortcutManager::Get().SaveToFile();
        m_ConflictWarning.clear();
    }

    if (!m_ConflictWarning.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_ConflictWarning.c_str());
    }

    ImGui::Spacing();

    auto& shortcuts = ShortcutManager::Get();
    const auto& actions = shortcuts.GetActions();

    // 1. 카테고리별 액션 분류
    std::map<std::string, std::vector<std::string>> categorizedActions;
    for (const auto& [id, action] : actions)
    {
        const std::string catKey = action.CategoryKey.empty() ? "EditorCommon.General" : action.CategoryKey;
        categorizedActions[catKey].push_back(id);
    }

    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;

    // 2. 카테고리별 CollapsingHeader 렌더링
    for (const auto& [catKey, actionIds] : categorizedActions)
    {
        std::vector<std::string> visibleIds;
        for (const auto& id : actionIds)
        {
            const auto it = actions.find(id);
            if (it == actions.end()) continue;
            const auto& action = it->second;

            const std::string displayName = action.DisplayNameKey.empty() ? id : LOCAL(action.DisplayNameKey);
            const std::string category = LOCAL(catKey);
            const ShortcutChord activeChord = shortcuts.GetActiveChord(id);
            const std::string chordText = activeChord.IsValid() ? activeChord.ToString() : LOCAL("EditorCommon.None");

            if (searchFilter.PassFilter(displayName.c_str()) || searchFilter.PassFilter(id.c_str()) ||
                searchFilter.PassFilter(category.c_str()) || searchFilter.PassFilter(chordText.c_str()))
            {
                visibleIds.push_back(id);
            }
        }

        if (visibleIds.empty()) continue;

        const std::string headerTitle = LOCAL(catKey) + " (" + std::to_string(visibleIds.size()) + ")###Header_" + catKey;
        if (ImGui::CollapsingHeader(headerTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::string tableId = "Table_" + catKey;
            if (ImGui::BeginTable(tableId.c_str(), 3, tableFlags))
            {
                ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColAction"), ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColShortcut"), ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColReset"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();

                for (const auto& id : visibleIds)
                {
                    const auto& action = actions.at(id);
                    const std::string displayName = action.DisplayNameKey.empty() ? id : LOCAL(action.DisplayNameKey);
                    const ShortcutChord activeChord = shortcuts.GetActiveChord(id);
                    const std::string chordText = activeChord.IsValid() ? activeChord.ToString() : LOCAL("EditorCommon.None");

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(displayName.c_str());
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", id.c_str());

                    ImGui::TableNextColumn();
                    ImGui::PushID(id.c_str());

                    const bool isListening = (m_ListeningActionId == id);
                    const std::string buttonLabel = isListening ? LOCAL("Shortcuts.PressKey") : "[" + chordText + "]";

                    if (isListening) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.5f, 0.1f, 1.0f));

                    if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1.0f, 0.0f)))
                    {
                        m_ListeningActionId = isListening ? "" : id;
                        m_ConflictWarning.clear();
                        ShortcutManager::Get().SetCapturing(!m_ListeningActionId.empty());
                    }

                    if (isListening) ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    const bool isModified = (activeChord != action.DefaultChord);
                    if (!isModified) ImGui::BeginDisabled();

                    if (ImGui::Button(LOCAL_CSTR("Shortcuts.Reset")))
                    {
                        shortcuts.ResetBinding(id);
                        shortcuts.SaveToFile();
                        m_ConflictWarning.clear();
                    }

                    if (!isModified) ImGui::EndDisabled();
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
            ImGui::Spacing();
        }
    }
}


void ShortcutSettingsWidget::HandleCapture()
{
    if (m_ListeningActionId.empty()) return;

    auto& shortcuts = ShortcutManager::Get();
    auto& input = Engine::InputManager::Get();

    // Escape 누르면 바인딩 취소
    if (input.IsKeyButtonDown(Engine::EKeyCode::Escape))
    {
        m_ListeningActionId.clear();
        m_ConflictWarning.clear();
        shortcuts.SetCapturing(false);
        return;
    }

    // Backspace 누르면 단축키 제거 (Unbound)
    if (input.IsKeyButtonDown(Engine::EKeyCode::Backspace))
    {
        shortcuts.Rebind(m_ListeningActionId, ShortcutChord());
        shortcuts.SaveToFile();
        m_ListeningActionId.clear();
        m_ConflictWarning.clear();
        shortcuts.SetCapturing(false);
        shortcuts.SuppressNextProcess();
        return;
    }

    const auto newChord = ShortcutChord::Capture(input);
    if (!newChord.IsValid()) return;

    if (const auto* action = shortcuts.FindAction(m_ListeningActionId))
    {
        const std::string conflictId = shortcuts.FindConflict(newChord, action->Context, m_ListeningActionId);
        if (!conflictId.empty())
        {
            const auto* conflictAction = shortcuts.FindAction(conflictId);
            const std::string conflictName = conflictAction && !conflictAction->DisplayNameKey.empty()
                ? LOCAL(conflictAction->DisplayNameKey) : conflictId;
            m_ConflictWarning = LOCAL_FMT("Shortcuts.ConflictWarning", newChord.ToString(), conflictName);
        }
        else
        {
            m_ConflictWarning.clear();
        }

        shortcuts.Rebind(m_ListeningActionId, newChord, true);
        shortcuts.SaveToFile();
    }

    m_ListeningActionId.clear();
    shortcuts.SetCapturing(false);
    shortcuts.SuppressNextProcess();
}

void ShortcutSettingsWidget::CancelCapture()
{
    if (!m_ListeningActionId.empty())
    {
        m_ListeningActionId.clear();
        ShortcutManager::Get().SetCapturing(false);
    }

    m_ConflictWarning.clear();
}
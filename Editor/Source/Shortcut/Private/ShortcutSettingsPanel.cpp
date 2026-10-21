#include "ShortcutSettingsPanel.h"
#include "Editor_Macros.h"
#include "InputManager.h"

ShortcutSettingsPanel::ShortcutSettingsPanel()
{
    m_Name = L"ShortcutSettings";
    m_DisplayNameKey = "Shortcuts.Title";
    m_Open = false; // 기본적으로 닫혀 있고 메뉴를 통해 오픈
}

void ShortcutSettingsPanel::Draw()
{
    if (!m_Open)
        return;

    ImGui::SetNextWindowSize(ImVec2(650.0f, 450.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(LOCAL_LABEL("Shortcuts.Title").c_str(), &m_Open, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    HandleKeyCapture();
    DrawHeader();
    ImGui::Separator();
    DrawActionTable();

    ImGui::End();
}

void ShortcutSettingsPanel::DrawHeader()
{
    m_Filter.Draw(LOCAL_CSTR("Shortcuts.Search"), 250.0f);
    ImGui::SameLine();

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
}

void ShortcutSettingsPanel::DrawActionTable()
{
    auto& shortcuts = ShortcutManager::Get();
    const auto& actions = shortcuts.GetActions();

    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit;

    if (ImGui::BeginTable("ShortcutTable", 4, tableFlags))
    {
        ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColAction"), ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColCategory"), ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColShortcut"), ImGuiTableColumnFlags_WidthFixed, 180.0f);
        ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColReset"), ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableHeadersRow();

        for (const auto& [id, action] : actions)
        {
            const std::string displayName = action.DisplayNameKey.empty() ? id : LOCAL(action.DisplayNameKey);
            const std::string category = action.CategoryKey.empty() ? "General" : LOCAL(action.CategoryKey);
            const ShortcutChord activeChord = shortcuts.GetActiveChord(id);
            const std::string chordText = activeChord.ToString();

            if (!m_Filter.PassFilter(displayName.c_str()) && !m_Filter.PassFilter(id.c_str()) &&
                !m_Filter.PassFilter(category.c_str()) && !m_Filter.PassFilter(chordText.c_str()))
            {
                continue;
            }

            ImGui::TableNextRow();

            // 1. 액션 이름
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(displayName.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", id.c_str());

            // 2. 카테고리
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", category.c_str());

            // 3. 단축키 버튼 (누르면 키 입력 대기 모드)
            ImGui::TableNextColumn();
            ImGui::PushID(id.c_str());

            const bool isListening = (m_ListeningActionId == id);
            std::string buttonLabel = isListening ? LOCAL("Shortcuts.PressKey") : "[" + chordText + "]";

            if (isListening)
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.0f, 1.0f));

            if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1.0f, 0.0f)))
            {
                m_ListeningActionId = isListening ? "" : id;
                m_ConflictWarning.clear();
            }

            if (isListening)
                ImGui::PopStyleColor();

            // 4. 개별 복원 버튼 (기본값과 다를 때만 활성)
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
}

void ShortcutSettingsPanel::HandleKeyCapture()
{
    if (m_ListeningActionId.empty())
        return;

    auto& input = Engine::InputManager::Get();

    // Escape 누르면 바인딩 취소
    if (input.IsKeyButtonDown(Engine::EKeyCode::Escape))
    {
        m_ListeningActionId.clear();
        m_ConflictWarning.clear();
        return;
    }

    // Backspace 누르면 단축키 제거 (Unbound)
    if (input.IsKeyButtonDown(Engine::EKeyCode::Backspace))
    {
        auto& shortcuts = ShortcutManager::Get();
        shortcuts.Rebind(m_ListeningActionId, ShortcutChord());
        shortcuts.SaveToFile();
        m_ListeningActionId.clear();
        m_ConflictWarning.clear();
        return;
    }

    // 보조키(Ctrl, Shift, Alt) 자체는 주 키로 캡처하지 않음
    uint8 modifiers = 0;
    if (input.IsCtrlPressed())  modifiers |= static_cast<uint8>(EModifierKey::Ctrl);
    if (input.IsShiftPressed()) modifiers |= static_cast<uint8>(EModifierKey::Shift);
    if (input.IsAltPressed())   modifiers |= static_cast<uint8>(EModifierKey::Alt);

    // 일반 키가 Down 되었는지 스캔
    for (uint32 k = 1; k < static_cast<uint32>(Engine::EKeyCode::Count); ++k)
    {
        const auto key = static_cast<Engine::EKeyCode>(k);

        // 보조키 자체는 주 키 입력에서 제외
        if (key == Engine::EKeyCode::LCtrl || key == Engine::EKeyCode::RCtrl ||
            key == Engine::EKeyCode::LShift || key == Engine::EKeyCode::RShift ||
            key == Engine::EKeyCode::LAlt || key == Engine::EKeyCode::RAlt)
        {
            continue;
        }

        if (input.IsKeyButtonDown(key))
        {
            const ShortcutChord newChord(key, modifiers);
            auto& shortcuts = ShortcutManager::Get();
            const auto* action = shortcuts.FindAction(m_ListeningActionId);

            if (action)
            {
                // 충돌 검사
                std::string conflictId = shortcuts.FindConflict(newChord, action->Context, m_ListeningActionId);
                if (!conflictId.empty())
                {
                    m_ConflictWarning = "Conflict: " + newChord.ToString() + " is already assigned to " + conflictId;
                }
                else
                {
                    m_ConflictWarning.clear();
                }

                shortcuts.Rebind(m_ListeningActionId, newChord);
                shortcuts.SaveToFile();
            }

            m_ListeningActionId.clear();
            break;
        }
    }
}
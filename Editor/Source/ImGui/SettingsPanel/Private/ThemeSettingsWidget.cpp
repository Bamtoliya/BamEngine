#include "ThemeSettingsWidget.h"
#include "EditorSettings.h"
#include "Editor_Includes.h"
#include "ImGuiManager.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"

void ThemeSettingsWidget::ReloadDraft(const EditorPreferences& preferences)
{
    m_DraftTheme = preferences.Theme;
    m_EditorDirty = false;
}

void ThemeSettingsWidget::ResetToDefaults(const EditorPreferences& defaults)
{
    m_DraftTheme = defaults.Theme;
    m_EditorDirty = true;
}

void ThemeSettingsWidget::WriteDraft(EditorPreferences& candidate) const
{
    candidate.Theme = m_DraftTheme;
}

void ThemeSettingsWidget::FinishApply()
{
    auto& imgui = ImGuiManager::Get();
    if (m_DraftTheme == "Custom")
        (void)imgui.SaveCustomStyle(imgui.GetCustomStylePath());

    imgui.ApplyTheme(m_DraftTheme);
    m_EditorDirty = false;
}

void ThemeSettingsWidget::Draw()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryTheme"));
    ImGui::Separator();
    ImGui::Spacing();
    // 1. 테마 프리셋
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.Theme"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        const std::string themeLabels[] = { LOCAL("Settings.ThemeBamEngine"), LOCAL("Settings.ThemeLight"),
            LOCAL("Settings.ThemeClassic"), LOCAL("Settings.ThemeCustom") };
        const char* themes[] = { themeLabels[0].c_str(), themeLabels[1].c_str(), themeLabels[2].c_str(), themeLabels[3].c_str() };
        int currentThemeIdx = 0;
        if (m_DraftTheme == "Light") currentThemeIdx = 1;
        else if (m_DraftTheme == "Classic") currentThemeIdx = 2;
        else if (m_DraftTheme == "Custom") currentThemeIdx = 3;
        ImGui::SetNextItemWidth(250.0f);
        if (ImGui::Combo(LOCAL_LABEL("Settings.Theme").c_str(), &currentThemeIdx, themes, IM_ARRAYSIZE(themes)))
        {
            if (currentThemeIdx == 1) m_DraftTheme = "Light";
            else if (currentThemeIdx == 2) m_DraftTheme = "Classic";
            else if (currentThemeIdx == 3) m_DraftTheme = "Custom";
            else m_DraftTheme = "Dark";
            ImGuiManager::Get().ApplyTheme(m_DraftTheme);
            m_EditorDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(LOCAL_LABEL("Settings.SaveCustomStyle").c_str()))
        {
            const auto stylePath = ImGuiManager::Get().GetCustomStylePath();
            if (ImGuiManager::Get().SaveCustomStyle(stylePath))
            {
                m_DraftTheme = "Custom";
                m_EditorDirty = true;
                NotificationManager::Get().AddNotification(LOCAL("Settings.CategoryTheme"), LOCAL("Settings.StyleSavedNotification"), ELogLevel::Info);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(LOCAL_LABEL("Settings.ResetCustomStyle").c_str()))
        {
            m_DraftTheme = "Dark";
            ImGuiManager::Get().ApplyDefaultDarkStyle();
            m_EditorDirty = true;
        }
        ImGui::Spacing();
    }
    // 2. ImGui 스타일 상세 커스터마이저
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.StyleEditor"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::TextDisabled(LOCAL_CSTR("Settings.StyleHelp"));
        ImGui::Spacing();
        ImGui::ShowStyleEditor(&ImGui::GetStyle());
        m_DraftTheme = "Custom";
        m_EditorDirty = true;
    }
}
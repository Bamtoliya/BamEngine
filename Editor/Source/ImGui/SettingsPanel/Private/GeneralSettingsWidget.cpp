#include "GeneralSettingsWidget.h"
#include "EditorSettings.h"
#include "Editor_Includes.h"
#include "FileDialogs.h"
#include "Functions.h"
#include "ImGuiManager.h"

void GeneralSettingsWidget::ReloadDraft(const EditorPreferences& preferences)
{
    m_DraftLanguage = preferences.Language == "English"
        ? ELocalizationLanguage::English : ELocalizationLanguage::Korean;
    m_DraftUIScale = ImGuiManager::Get().GetUIScale();
    m_DraftCustomFontPath = preferences.CustomFontPath;
    m_DraftFontSize = preferences.FontSize >= 10.0f ? preferences.FontSize : 18.0f;
    m_DraftAutoSaveEnabled = preferences.AutoSaveEnabled;
    m_DraftAutoSaveIntervalMinutes = preferences.AutoSaveIntervalMinutes;

    m_LanguageDirty = false;
    m_UIScaleDirty = false;
    m_FontDirty = false;
    m_EditorDirty = false;
}

void GeneralSettingsWidget::ResetToDefaults(const EditorPreferences& defaults)
{
    m_DraftLanguage = defaults.Language == "English"
        ? ELocalizationLanguage::English : ELocalizationLanguage::Korean;
    m_DraftUIScale = defaults.UIScale;
    m_DraftCustomFontPath = defaults.CustomFontPath;
    m_DraftFontSize = defaults.FontSize;
    m_DraftAutoSaveEnabled = defaults.AutoSaveEnabled;
    m_DraftAutoSaveIntervalMinutes = defaults.AutoSaveIntervalMinutes;

    m_LanguageDirty = true;
    m_UIScaleDirty = true;
    m_FontDirty = true;
    m_EditorDirty = true;
}

void GeneralSettingsWidget::WriteDraft(EditorPreferences& candidate) const
{
    if (m_UIScaleDirty)
        candidate.UIScale = m_DraftUIScale;

    candidate.AutoSaveEnabled = m_DraftAutoSaveEnabled;
    candidate.AutoSaveIntervalMinutes = m_DraftAutoSaveIntervalMinutes;
    candidate.CustomFontPath = m_DraftCustomFontPath;
    candidate.FontSize = m_DraftFontSize;
}

void GeneralSettingsWidget::FinishApply(bool applied)
{
    if (!applied)
        return;

    ImGuiManager::Get().UpdateFontScale(m_DraftFontSize);
    m_UIScaleDirty = false;
    m_FontDirty = false;
    m_EditorDirty = false;
}

void GeneralSettingsWidget::Draw()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryGeneral"));
    ImGui::Separator();
    ImGui::Spacing();
    // 1. 언어 및 로캘
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionLanguage"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        const char* languages[] = { "한국어 (Korean)", "English" };
        int currentLangIdx = (m_DraftLanguage == ELocalizationLanguage::English) ? 1 : 0;
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::Combo(LOCAL_LABEL("Settings.Language").c_str(), &currentLangIdx, languages, IM_ARRAYSIZE(languages)))
        {
            m_DraftLanguage = (currentLangIdx == 1) ? ELocalizationLanguage::English : ELocalizationLanguage::Korean;
            m_LanguageDirty = true;
        }
        ImGui::Spacing();
    }
    // 2. 외형 및 디스플레이 배율 (오직 UI 배율만 전담)
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionAppearance"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat(LOCAL_LABEL("ToolBar.UIScale").c_str(), &m_DraftUIScale, EditorPreferences::MinUIScale, EditorPreferences::MaxUIScale, "%.2fx"))
            m_UIScaleDirty = true;
        ImGui::Spacing();
    }
    // 3. 폰트 및 타이포그래피 (독립 헤더)
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionFont"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::TextUnformatted(LOCAL_CSTR("Settings.CustomFont"));
        ImGui::TextDisabled("%s", m_DraftCustomFontPath.empty() ? "System Default (malgun.ttf)" : m_DraftCustomFontPath.c_str());
        if (ImGui::Button(LOCAL_LABEL("Settings.FontBrowse").c_str()))
        {
            std::wstring selectedPath;
            if (FileDialogs::OpenFileDialog(selectedPath, { { Engine::StrToWStr(LOCAL("FileDialog.FontFiles")), L"*.ttf;*.otf" } }))
            {
                m_DraftCustomFontPath = WStrToStr(selectedPath);
                m_FontDirty = true;
            }
        }
        if (!m_DraftCustomFontPath.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button(LOCAL_LABEL("Settings.FontReset").c_str()))
            {
                m_DraftCustomFontPath.clear();
                m_FontDirty = true;
            }
        }
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat(LOCAL_LABEL("Settings.FontSize").c_str(), &m_DraftFontSize, 12.0f, 32.0f, "%.0f px"))
            m_FontDirty = true;
        ImGui::Spacing();
    }
    // 4. 자동 저장
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionAutoSave"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        if (ImGui::Checkbox(LOCAL_LABEL("Settings.AutoSaveEnabled").c_str(), &m_DraftAutoSaveEnabled))
            m_EditorDirty = true;
        if (m_DraftAutoSaveEnabled)
        {
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderInt(LOCAL_LABEL("Settings.AutoSaveInterval").c_str(), &m_DraftAutoSaveIntervalMinutes, 1, 60, "%d min"))
                m_EditorDirty = true;
        }
        ImGui::Spacing();
    }
}
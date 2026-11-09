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
    if (m_LanguageDirty)
        candidate.Language = m_DraftLanguage == ELocalizationLanguage::English ? "English" : "Korean";

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
    m_LanguageDirty = false;
    m_UIScaleDirty = false;
    m_FontDirty = false;
    m_EditorDirty = false;
}

void GeneralSettingsWidget::Draw()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryGeneral"));
    ImGui::Separator();
    ImGui::Spacing();

    const EditorUI::PropertyTableStyle tableStyle;

    // 1. 언어 및 로캘
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionLanguage"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (EditorUI::BeginPropertyTable("##GeneralLangTable", tableStyle))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(LOCAL_CSTR("Settings.Language"));

            ImGui::TableSetColumnIndex(1);
            const char* languages[] = { "한국어 (Korean)", "English" };
            int currentLangIdx = (m_DraftLanguage == ELocalizationLanguage::English) ? 1 : 0;
            ImGui::SetNextItemWidth(240.0f);
            if (ImGui::Combo("###Language", &currentLangIdx, languages, IM_ARRAYSIZE(languages)))
            {
                m_DraftLanguage = (currentLangIdx == 1) ? ELocalizationLanguage::English : ELocalizationLanguage::Korean;
                m_LanguageDirty = true;
            }

            EditorUI::EndPropertyTable();;
        }
        ImGui::Spacing();
    }

    // 2. 외형 및 디스플레이 배율 (오직 UI 배율만 전담)
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionAppearance"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (EditorUI::BeginPropertyTable("##GeneralAppearanceTable", tableStyle))
        {
            EditorUI::PropertyRow("ToolBar.UIScale");
            ImGui::SetNextItemWidth(240.0f);
            if (ImGui::SliderFloat("###UIScale", &m_DraftUIScale,
                EditorPreferences::MinUIScale, EditorPreferences::MaxUIScale, "%.2fx"))
            {
                m_UIScaleDirty = true;
            }

            EditorUI::EndPropertyTable();;
        }
        ImGui::Spacing();
    }

    // 3. 폰트 및 타이포그래피 (독립 헤더)
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionFont"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (EditorUI::BeginPropertyTable("##GeneralFontTable", tableStyle))
        {
            // Font Path & Browse
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(LOCAL_CSTR("Settings.CustomFont"));

            ImGui::TableSetColumnIndex(1);
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s", m_DraftCustomFontPath.empty() ? "System Default (malgun.ttf)" : m_DraftCustomFontPath.c_str());
            ImGui::SameLine();
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

            // Font Size
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(LOCAL_CSTR("Settings.FontSize"));

            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(240.0f);
            if (ImGui::SliderFloat("###FontSize", &m_DraftFontSize, 12.0f, 32.0f, "%.0f px"))
                m_FontDirty = true;

            EditorUI::EndPropertyTable();;
        }
        ImGui::Spacing();
    }

    // 4. 자동 저장
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionAutoSave"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (EditorUI::BeginPropertyTable("##GeneralAutoSaveTable", tableStyle))
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(LOCAL_CSTR("Settings.AutoSaveEnabled"));

            ImGui::TableSetColumnIndex(1);
            if (ImGui::Checkbox("###AutoSaveEnabled", &m_DraftAutoSaveEnabled))
                m_EditorDirty = true;

            if (m_DraftAutoSaveEnabled)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(LOCAL_CSTR("Settings.AutoSaveInterval"));

                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(240.0f);
                if (ImGui::SliderInt("###AutoSaveInterval", &m_DraftAutoSaveIntervalMinutes, 1, 60, "%d min"))
                    m_EditorDirty = true;
            }

            EditorUI::EndPropertyTable();
        }
        ImGui::Spacing();
    }
}
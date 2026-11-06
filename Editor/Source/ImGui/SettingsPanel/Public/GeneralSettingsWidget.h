#pragma once

#include "Editor_Includes.h"
#include "LocalizationManager.h"

BEGIN(Editor)
struct EditorPreferences;

class GeneralSettingsWidget
{
public:
    void ReloadDraft(const EditorPreferences& preferences);
    void ResetToDefaults(const EditorPreferences& defaults);
    void WriteDraft(EditorPreferences& candidate) const;
    void FinishApply(bool applied);
    void Draw();

    [[nodiscard]] bool IsDirty() const
    {
        return m_LanguageDirty || m_UIScaleDirty || m_FontDirty || m_EditorDirty;
    }

private:
    ELocalizationLanguage m_DraftLanguage = ELocalizationLanguage::Korean;
    float m_DraftUIScale = 1.0f;
    string m_DraftCustomFontPath;
    float m_DraftFontSize = 18.0f;
    bool m_DraftAutoSaveEnabled = true;
    int32 m_DraftAutoSaveIntervalMinutes = 5;

    bool m_LanguageDirty = false;
    bool m_UIScaleDirty = false;
    bool m_FontDirty = false;
    bool m_EditorDirty = false;
};
END
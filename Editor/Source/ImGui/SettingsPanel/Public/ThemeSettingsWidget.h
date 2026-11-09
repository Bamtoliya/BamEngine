#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
struct EditorPreferences;

class ThemeSettingsWidget
{
public:
    void ReloadDraft(const EditorPreferences& preferences);
    void ResetToDefaults(const EditorPreferences& defaults);
    [[nodiscard]] bool SaveDraftStyle() const;
    void WriteDraft(EditorPreferences& candidate) const;
    void FinishApply(bool applied);
    void Draw();

    [[nodiscard]] bool IsDirty() const { return m_EditorDirty; }
private:
    void DrawStyleDraft();
private:
    string m_DraftTheme = "Dark";
    string m_AppliedTheme = "Dark";
    ImGuiStyle m_DraftStyle{};
    ImGuiStyle m_OriginalStyle{};
    bool m_StyleDirty = false;
    bool m_EditorDirty = false;
};
END
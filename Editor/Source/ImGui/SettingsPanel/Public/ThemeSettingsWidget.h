#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
struct EditorPreferences;

class ThemeSettingsWidget
{
public:
    void ReloadDraft(const EditorPreferences& preferences);
    void ResetToDefaults(const EditorPreferences& defaults);
    void WriteDraft(EditorPreferences& candidate) const;
    void FinishApply();
    void Draw();

    [[nodiscard]] bool IsDirty() const { return m_EditorDirty; }

private:
    string m_DraftTheme = "Dark";
    bool m_EditorDirty = false;
};
END
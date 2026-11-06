#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class EditorSettingsService;
class ProjectSettingsWidget
{
public:
    void ReloadDraft(const EditorSettingsService& settings);
    void Apply(EditorSettingsService& settings);
    void Draw(const EditorSettingsService& settings);

    [[nodiscard]] bool IsDirty() const { return m_Dirty; }

private:
    string m_DraftProjectName;
    string m_DraftStartupScenePath;
    string m_DraftAssetDirectory;
    string m_DraftShaderSourceDirectory;
    bool m_Dirty = false;
};
END
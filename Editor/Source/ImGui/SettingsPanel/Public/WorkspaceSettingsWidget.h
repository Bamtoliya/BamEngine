#pragma once

#include "Editor_Includes.h"
#include "EditorSettings.h"
#include <map>
#include <string>

BEGIN(Editor)
class EditorSettingsService;
class WorkspaceSettingsWidget
{
public:
    void ReloadDraft(const EditorSettingsService& settings);
    void SaveWorkspace(EditorSettingsService& settings);
    void ApplyViewportDrafts(EditorSettingsService& settings);
    void Update(EditorSettingsService& settings);
    void Draw(EditorSettingsService& settings);

    [[nodiscard]] bool IsDirty() const
    {
        return m_WorkspaceDirty || !m_ViewportDrafts.empty();
    }

private:
    f32 m_DraftThumbnailSize = 64.0f;
    bool m_WorkspaceDirty = false;
    wstring m_SelectedViewportName;
    map<std::wstring, ViewportWorkspace> m_ViewportDrafts;
};
END
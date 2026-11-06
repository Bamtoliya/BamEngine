#pragma once

#include "Editor_Includes.h"

BEGIN(Editor)
class EditorSettingsService;
class GraphicSettingsWidget
{
public:
    void ReloadDraft(const EditorSettingsService& settings);
    void Apply(EditorSettingsService& settings);
    void Update(EditorSettingsService& settings);
    void Draw();
    void DrawWindowConfirmation(EditorSettingsService& settings);

    [[nodiscard]] bool IsDirty() const
    {
        return m_WindowDirty || m_TargetFPSDirty || m_VSyncDirty;
    }

private:
    int32 m_DraftWindowWidth = 1280;
    int32 m_DraftWindowHeight = 720;
    int32 m_DraftTargetFPS = 0;
    bool m_DraftFullscreen = false;
    bool m_DraftVSync = true;
    bool m_WindowDirty = false;
    bool m_TargetFPSDirty = false;
    bool m_VSyncDirty = false;
};
END
#pragma once

#include "LogEntry.h"
#include "Editor_Includes.h"

BEGIN(Editor)
struct EditorPreferences;

class EditorEnvironmentSettingsWidget
{
public:
    void ReloadDraft(const EditorPreferences& preferences);
    void ResetToDefaults(const EditorPreferences& defaults);
    void WriteDraft(EditorPreferences& candidate) const;
    void FinishApply(bool applied);
    void Draw();

    [[nodiscard]] bool IsDirty() const
    {
        return m_EditorDirty || m_PreviewLightingDirty;
    }

private:
    f32 m_DraftCameraSpeed = 100.0f;
    f32 m_DraftCameraFOV = 60.0f;
    f32 m_DraftGizmoSnapTranslation = 1.0f;
    f32 m_DraftGizmoSnapRotation = 15.0f;
    f32 m_DraftGizmoSnapScale = 0.1f;
    ELogLevel m_DraftLogLevel = ELogLevel::Info;
    array<f32, 3> m_DraftPreviewLightIntensity{};

    bool m_EditorDirty = false;
    bool m_PreviewLightingDirty = false;
};
END
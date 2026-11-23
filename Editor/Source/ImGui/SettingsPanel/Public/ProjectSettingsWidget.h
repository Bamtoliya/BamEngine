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
    void DrawMetadata(const EditorSettingsService& settings);
    void DrawRendering();

    int32 m_DraftRaytracedShadow = 0;
    int32 m_DraftRaytracedReflection = 0;
    int32 m_DraftTemporalAA = 0;
    int32 m_DraftProbeGI = 0;
    int32 m_DraftStreamingBudget = 0;

    string m_DraftProjectName;
    string m_DraftStartupScenePath;
    string m_DraftAssetDirectory;
    string m_DraftShaderSourceDirectory;
    bool m_Dirty = false;
};
END
#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include "SettingsWidgets.h"

BEGIN(Editor)

enum class EUnifiedSettingsCategory
{
    General,
    Theme,
    Graphic,
    Editor,
    Workspace,
    Shortcuts,
    ProjectSettings,
    InputActions,
    Count
};

class UnifiedSettingsPanel final : public IImGui
{
public:
    UnifiedSettingsPanel();
    ~UnifiedSettingsPanel() override = default;

    void Update(f32 dt) override;
    void Draw() override;
    bool ShouldUpdateWhenClosed() const override { return true; }
    void SetActiveCategory(EUnifiedSettingsCategory category) { m_ActiveCategory = category; }
    void OpenWithCategory(EUnifiedSettingsCategory category) { m_ActiveCategory = category; m_Open = true; }
    void CaptureState(EditorWorkspace& workspace) const override;
    void RestoreState(const EditorWorkspace& workspace) override;

private:
    void ResetDrafts();
    void ResetToDefaults();
    void ApplySettings();
    bool IsAnyDirty() const;

    void DrawSidebar();
    void DrawContentArea();
    void DrawFooter();
    void DrawSettingsStorageStatus() const;

private:
    bool m_StartupSettingsNoticePending = true;
    EUnifiedSettingsCategory m_ActiveCategory = EUnifiedSettingsCategory::General;
    ImGuiTextFilter m_SearchFilter;
    float m_SidebarWidth = 250.0f;
    bool m_DraftsInitialized = false;

    GeneralSettingsWidget m_General;
    ThemeSettingsWidget m_Theme;
    GraphicSettingsWidget m_Graphics;
    EditorEnvironmentSettingsWidget m_EditorEnvironment;
    WorkspaceSettingsWidget m_Workspace;
    ShortcutSettingsWidget m_Shortcuts;
    ProjectSettingsWidget m_ProjectSettings;
    InputActionSettingsWidget m_InputActions;
};

END
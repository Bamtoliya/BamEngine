#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include "LocalizationManager.h"
#include "Logger.h"

BEGIN(Editor)
struct ViewportWorkspace;
enum class EUnifiedSettingsCategory
{
    General,
    Theme,
    Graphic,
    Editor,
    Workspace,
    Shortcuts,
    ProjectSettings,
    Count
};

class UnifiedSettingsPanel final : public IImGui
{
public:
    UnifiedSettingsPanel();
    ~UnifiedSettingsPanel() override = default;

    void Update(f32 dt) override;
    void Draw() override;

    void SetActiveCategory(EUnifiedSettingsCategory category) { m_ActiveCategory = category; }
    void OpenWithCategory(EUnifiedSettingsCategory category) { m_ActiveCategory = category; m_Open = true; }
    virtual void CaptureState(EditorWorkspace& workspace) const override;
    virtual void RestoreState(const EditorWorkspace& workspace) override;

private:
    void ResetDrafts();
    void ResetToDefaults();
    void ApplySettings();
    bool IsAnyDirty() const;

    void DrawSidebar();
    void DrawContentArea();
    void DrawFooter();
    void DrawWindowConfirmation();

    // 카테고리별 뷰
    void DrawCategoryGeneral();
    void DrawCategoryTheme();
    void DrawCategoryGraphic();
    void DrawCategoryEditor();
    void DrawCategoryWorkspace();
    void DrawCategoryShortcuts();
    void DrawCategoryProjectSettings();

    // Shortcuts 전용 키 캡처 헬퍼
    void HandleKeyCapture();

private:
    EUnifiedSettingsCategory m_ActiveCategory = EUnifiedSettingsCategory::General;
    ImGuiTextFilter m_SearchFilter;
    float m_SidebarWidth = 250.0f;
    bool m_DraftsInitialized = false;

    // 세부 Dirty 플래그
    bool m_LanguageDirty = false;
    bool m_UIScaleDirty = false;
    bool m_WindowDirty = false;
    bool m_TargetFPSDirty = false;
    bool m_VSyncDirty = false;
    bool m_EditorDirty = false;
    bool m_WorkspaceDirty = false;
    bool m_FontDirty = false;

    // General 초안 변수
    ELocalizationLanguage m_DraftLanguage = ELocalizationLanguage::Korean;
    float m_DraftUIScale = 1.0f;
    std::string m_DraftCustomFontPath;
    float m_DraftFontSize = 18.0f;

    // Graphic 초안 변수
    int32 m_DraftWindowWidth = 1280;
    int32 m_DraftWindowHeight = 720;
    bool m_DraftFullscreen = false;
    int32 m_DraftTargetFPS = 0;
    bool m_DraftVSync = true;

    // Shortcuts 전용 상태
    std::string m_ListeningActionId;
    std::string m_ConflictWarning;

    // Editor 환경 초안 변수
    float m_DraftCameraSpeed = 100.0f;
    float m_DraftGizmoSnapTranslation = 1.0f;
    float m_DraftGizmoSnapRotation = 15.0f;
    float m_DraftGizmoSnapScale = 0.1f;
    Engine::ELogLevel m_DraftLogLevel = Engine::ELogLevel::Info;

    // Workspace 환경 초안 변수
    float m_DraftThumbnailSize = 64.0f;
    std::wstring m_SelectedViewportName;
    std::map<std::wstring, ViewportWorkspace> m_ViewportDrafts;

    // ProjectSettings 초안 변수
    std::string m_DraftProjectName;
    std::string m_DraftStartupScenePath;
    bool m_ProjectSettingsDirty = false;
    std::string m_DraftAssetDirectory;       
    std::string m_DraftShaderSourceDirectory;

    std::string m_DraftTheme = "Dark";
    bool m_DraftAutoSaveEnabled = true;
    int32 m_DraftAutoSaveIntervalMinutes = 5;
    float m_DraftCameraFOV = 60.0f;

    //Preview 변수
    std::array<float, 3> m_DraftPreviewLightIntensity{};
    bool m_PreviewLightingDirty = false;
};

END
#include "UnifiedSettingsPanel.h"
#include "LocalizationManager.h"
#include "EditorSettingsService.h"
#include "ImGuiManager.h"
#include "Application.h"
#include "TimeManager.h"
#include "NotificationManager.h"
#include "ShortcutManager.h"
#include "InputManager.h"
#include "SceneViewportPanel.h"
#include "BaseViewportPanel.h"
#include "FileDialogs.h"

BEGIN(Editor)

UnifiedSettingsPanel::UnifiedSettingsPanel()
{
    m_Name = L"UnifiedSettings";
    m_DisplayNameKey = "Settings.Title";
    m_Open = false;
}

bool UnifiedSettingsPanel::IsAnyDirty() const
{
    return m_LanguageDirty || m_UIScaleDirty || m_WindowDirty || m_TargetFPSDirty || m_VSyncDirty ||
        m_EditorDirty || m_WorkspaceDirty || m_ProjectSettingsDirty || m_FontDirty || !m_ViewportDrafts.empty();
}

void UnifiedSettingsPanel::Update(f32 dt)
{
    auto& settings = EditorSettingsService::Get();

    if (const auto result = settings.TakeVSyncRequestResult())
    {
        if (result->Applied)
        {
            m_VSyncDirty = false;
            if (const auto actual = settings.GetEditorVSync())
                m_DraftVSync = *actual;
        }

        if (result->Applied && result->SaveStatus == EEditorSettingsSaveStatus::Saved)
            NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"), LOCAL("GraphicsSettings.VSyncSaved"), ELogLevel::Info);
        else
            ImGuiManager::Get().NotifySettingsChange(*result);
    }

    if (const auto result = settings.TakeWindowRequestResult())
    {
        const auto status = result->Status;
        const bool refresh = (status == EEditorWindowRequestStatus::Confirmed || status == EEditorWindowRequestStatus::Cancelled || status == EEditorWindowRequestStatus::ConfirmationTimedOut);

        if (refresh && result->ActualState.HasWindowSize)
        {
            m_DraftWindowWidth = result->ActualState.WindowWidth;
            m_DraftWindowHeight = result->ActualState.WindowHeight;
            m_DraftFullscreen = result->ActualState.WindowFullscreen;
            m_WindowDirty = false;
        }

        if (status == EEditorWindowRequestStatus::Confirmed)
        {
            if (result->SaveStatus == EEditorSettingsSaveStatus::Saved)
                NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"), LOCAL("EditorSettings.WindowConfirmed"), ELogLevel::Info);
            else
                ImGuiManager::Get().NotifySettingsChange({ true, result->SaveStatus });
        }
        else if (status == EEditorWindowRequestStatus::Cancelled)
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"), LOCAL("EditorSettings.WindowReverted"), ELogLevel::Info);
        }
        else if (status == EEditorWindowRequestStatus::ConfirmationTimedOut)
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"), LOCAL("EditorSettings.WindowConfirmationTimedOut"), ELogLevel::Warn);
        }
        else if (status == EEditorWindowRequestStatus::TimedOut)
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"), LOCAL("EditorSettings.WindowApplyTimedOut"), ELogLevel::Error);
        }
        else if (status == EEditorWindowRequestStatus::RestoreFailed)
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"), LOCAL("EditorSettings.WindowRestoreFailed"), ELogLevel::Error);
        }
        else if (status == EEditorWindowRequestStatus::SubmissionFailed)
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"), LOCAL("EditorSettings.WindowRequestFailed"), ELogLevel::Error);
        }
    }

    // 뷰포트 해상도 비동기 변경 결과 처리
    if (const auto vpResult = settings.TakeViewportResolutionResult())
    {
        if (vpResult->Change.Applied)
            m_ViewportDrafts.erase(vpResult->Name);
        if (!vpResult->RenderTargetsValid)
        {
            NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"), LOCAL("GraphicsSettings.ViewportRestoreFailed"), ELogLevel::Error);
        }
        else if (vpResult->Change.Applied && vpResult->Change.SaveStatus == EEditorSettingsSaveStatus::Saved)
        {
            NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"), LOCAL("GraphicsSettings.ViewportSaved"), ELogLevel::Info);
        }
        else
        {
            ImGuiManager::Get().NotifySettingsChange(vpResult->Change);
        }
    }
}

void UnifiedSettingsPanel::ResetDrafts()
{
    auto& settings = EditorSettingsService::Get();
    const auto& prefs = settings.GetPreferences();

    // General
    m_DraftLanguage = (prefs.Language == "English") ? ELocalizationLanguage::English : ELocalizationLanguage::Korean;
    m_DraftUIScale = ImGuiManager::Get().GetUIScale();
    m_DraftCustomFontPath = prefs.CustomFontPath;
    m_DraftFontSize = (prefs.FontSize >= 10.0f) ? prefs.FontSize : 18.0f;
    m_FontDirty = false;

    // Graphic
    Application::Get().GetWindowSize(&m_DraftWindowWidth, &m_DraftWindowHeight);
    m_DraftFullscreen = Application::Get().IsFullscreen();
    m_DraftTargetFPS = static_cast<int32>(Engine::TimeManager::Get().GetTargetFPS());

    if (const auto actualVSync = settings.GetEditorVSync())
        m_DraftVSync = *actualVSync;
    else
        m_DraftVSync = prefs.VSync;

    // Editor
    if (auto* vp = SceneViewportPanel::GetActiveSceneViewport())
    {
        m_DraftGizmoSnapTranslation = prefs.GizmoSnapTranslation;
        m_DraftGizmoSnapRotation = prefs.GizmoSnapRotation;
        m_DraftGizmoSnapScale = prefs.GizmoSnapScale;
    }

    m_LanguageDirty = false;
    m_UIScaleDirty = false;
    m_WindowDirty = false;
    m_TargetFPSDirty = false;
    m_VSyncDirty = false;
    m_EditorDirty = false;
    m_DraftsInitialized = true;
    m_FontDirty = false;

    // Workspace
    m_DraftThumbnailSize = settings.GetWorkspace().ContentBrowserThumbnailSize;
    m_WorkspaceDirty = false;

    // ProjectSettings
    const auto& proj = settings.GetProjectSettings();
    m_DraftProjectName = proj.ProjectName;
    m_DraftStartupScenePath = proj.StartupScenePath;
    m_ProjectSettingsDirty = false;
    m_ViewportDrafts.clear();
    m_DraftAssetDirectory = proj.AssetDirectory;
    m_DraftShaderSourceDirectory = proj.ShaderSourceDirectory;   

    // 테마 및 자동 저장
    m_DraftTheme = prefs.Theme;
    m_DraftAutoSaveEnabled = prefs.AutoSaveEnabled;
    m_DraftAutoSaveIntervalMinutes = prefs.AutoSaveIntervalMinutes;
    // 카메라
    m_DraftCameraSpeed = prefs.CameraSpeed;
    m_DraftCameraFOV = prefs.CameraFOV;
}

void UnifiedSettingsPanel::ResetToDefaults()
{
    EditorPreferences defPrefs;
    m_DraftLanguage = (defPrefs.Language == "English") ? ELocalizationLanguage::English : ELocalizationLanguage::Korean;
    m_DraftUIScale = defPrefs.UIScale;
    m_DraftCustomFontPath = defPrefs.CustomFontPath;
    m_DraftFontSize = defPrefs.FontSize;
    m_DraftTheme = defPrefs.Theme;
    m_DraftAutoSaveEnabled = defPrefs.AutoSaveEnabled;
    m_DraftAutoSaveIntervalMinutes = defPrefs.AutoSaveIntervalMinutes;
    m_DraftCameraSpeed = defPrefs.CameraSpeed;
    m_DraftCameraFOV = defPrefs.CameraFOV;
    m_DraftGizmoSnapTranslation = defPrefs.GizmoSnapTranslation;
    m_DraftGizmoSnapRotation = defPrefs.GizmoSnapRotation;
    m_DraftGizmoSnapScale = defPrefs.GizmoSnapScale;
    m_LanguageDirty = true;
    m_UIScaleDirty = true;
    m_FontDirty = true;
    m_EditorDirty = true;
}

void UnifiedSettingsPanel::ApplySettings()
{
    auto& settings = EditorSettingsService::Get();

    if (m_LanguageDirty || m_UIScaleDirty || m_FontDirty || m_EditorDirty)
    {
        EditorPreferences candidate = settings.GetPreferences();
        candidate.Theme = m_DraftTheme;
        candidate.AutoSaveEnabled = m_DraftAutoSaveEnabled;
        candidate.AutoSaveIntervalMinutes = m_DraftAutoSaveIntervalMinutes;
        candidate.CameraSpeed = m_DraftCameraSpeed;
        candidate.CameraFOV = m_DraftCameraFOV;
        candidate.CustomFontPath = m_DraftCustomFontPath;
        candidate.FontSize = m_DraftFontSize;
        candidate.GizmoSnapTranslation = m_DraftGizmoSnapTranslation;
        candidate.GizmoSnapRotation = m_DraftGizmoSnapRotation;
        candidate.GizmoSnapScale = m_DraftGizmoSnapScale;
        const auto result = settings.SetEditorPreferences(candidate);
        ImGuiManager::Get().NotifySettingsChange(result);
        // 폰트 크기 즉각 실시간 반영!
        ImGuiManager::Get().UpdateFontScale(m_DraftFontSize);
        // 테마 및 커스텀 스타일 저장 & 적용
        if (m_DraftTheme == "Custom")
        {
            (void)ImGuiManager::Get().SaveCustomStyle(ImGuiManager::Get().GetCustomStylePath());
        }
        ImGuiManager::Get().ApplyTheme(m_DraftTheme);
        m_EditorDirty = false;
        m_FontDirty = false;
    }

    if (m_TargetFPSDirty)
    {
        const auto fpsResult = settings.SetEditorTargetFPS(m_DraftTargetFPS);
        ImGuiManager::Get().NotifySettingsChange(fpsResult);
        if (fpsResult.Applied) m_TargetFPSDirty = false;
    }

    if (m_WindowDirty && !settings.IsWindowChangePending() && !settings.IsVSyncChangePending())
    {
        const EditorWindowRequest windowReq{ m_DraftWindowWidth, m_DraftWindowHeight, m_DraftFullscreen };
        if (windowReq.HasValidValues())
        {
            const auto winResult = settings.RequestWindowChange(windowReq);
            if (winResult != EResult::Success)
                NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"), LOCAL("EditorSettings.WindowRequestRejected"), ELogLevel::Error);
        }
    }

    if (m_VSyncDirty && !settings.IsWindowChangePending() && !settings.IsVSyncChangePending())
    {
        const auto vsyncResult = settings.RequestVSyncChange(m_DraftVSync);
        if (vsyncResult != EResult::Success)
            NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"), LOCAL("GraphicsSettings.VSyncRequestRejected"), ELogLevel::Warn);
    }

    if (m_WorkspaceDirty)
    {
        // Workspace.json 저장 요청
        settings.SaveWorkspace();
        m_WorkspaceDirty = false;
    }

    if (m_ProjectSettingsDirty)
    {
        auto proj = settings.GetProjectSettings();
        proj.ProjectName = m_DraftProjectName;
        proj.StartupScenePath = m_DraftStartupScenePath;
        proj.AssetDirectory = m_DraftAssetDirectory;
        proj.ShaderSourceDirectory = m_DraftShaderSourceDirectory;
        settings.SaveProjectSettings(proj);
        m_ProjectSettingsDirty = false;
    }

    if (!m_ViewportDrafts.empty())
    {
        for (const auto& [name, draft] : m_ViewportDrafts)
            (void)settings.RequestViewportResolutionChange(name, draft);
    }
}

void UnifiedSettingsPanel::Draw()
{
    DrawWindowConfirmation();

    if (!m_Open)
    {
        m_DraftsInitialized = false;
        if (!m_ListeningActionId.empty())
        {
            m_ListeningActionId.clear();
            ShortcutManager::Get().SetCapturing(false);
        }
        m_ConflictWarning.clear();
        return;
    }

    if (!m_DraftsInitialized)
    {
        ResetDrafts();
    }

    if (m_ActiveCategory == EUnifiedSettingsCategory::Shortcuts)
    {
        HandleKeyCapture();
    }

    ImGui::SetNextWindowSize(ImVec2(860.0f, 580.0f), ImGuiCond_FirstUseEver);
    const std::string windowTitle = LOCAL("Settings.Title") + "###UnifiedSettingsWindow";

    const ImGuiWindowFlags windowFlags = IsAnyDirty() ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None;

    if (!ImGui::Begin(windowTitle.c_str(), &m_Open, windowFlags))
    {
        ImGui::End();
        return;
    }

    m_SearchFilter.Draw(LOCAL_CSTR("Shortcuts.Search"), 300.0f);
    ImGui::Separator();

    const float footerHeight = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    const float contentHeight = ImGui::GetContentRegionAvail().y - footerHeight;

    // 1. 좌측 사이드바 영역 (가로 스크롤바 플래그 적용)
    ImGui::BeginChild("SettingsSidebarRegion", ImVec2(m_SidebarWidth, contentHeight), true, ImGuiWindowFlags_HorizontalScrollbar);
    DrawSidebar();
    ImGui::EndChild();

    ImGui::SameLine();

    // 2. 사이드바 크기 조절 스플리터 (드래그하여 크기 조절)
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorActive));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SeparatorHovered));
    ImGui::Button("##SidebarSplitter", ImVec2(4.0f, contentHeight));
    ImGui::PopStyleColor(3);

    if (ImGui::IsItemActive())
    {
        m_SidebarWidth += ImGui::GetIO().MouseDelta.x;
        const float minWidth = 180.0f;
        const float maxWidth = (std::max)(minWidth, ImGui::GetWindowWidth() * 0.5f);
        if (m_SidebarWidth < minWidth) m_SidebarWidth = minWidth;
        if (m_SidebarWidth > maxWidth) m_SidebarWidth = maxWidth;
    }
    if (ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    ImGui::SameLine();

    // 3. 우측 상세 콘텐츠 영역
    ImGui::BeginChild("SettingsContentRegion", ImVec2(0.0f, contentHeight), true);
    DrawContentArea();
    ImGui::EndChild();

    ImGui::Separator();
    DrawFooter();

    ImGui::End();
}

void UnifiedSettingsPanel::DrawSidebar()
{
    auto drawCategoryItem = [this](EUnifiedSettingsCategory cat, const char* icon, const char* labelKey)
        {
            const bool isSelected = (m_ActiveCategory == cat);
            const std::string label = std::string(icon) + "  " + LOCAL(labelKey);

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

            ImGui::TreeNodeEx((void*)(intptr_t)cat, flags, "%s", label.c_str());
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            {
                m_ActiveCategory = cat;
                if (!m_ListeningActionId.empty())
                {
                    m_ListeningActionId.clear();
                    ShortcutManager::Get().SetCapturing(false);
                }
                m_ConflictWarning.clear();
            }
        };

    // 1. 시스템 및 화면 그룹
    if (ImGui::TreeNodeEx("SidebarGroupSystem", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", LOCAL_CSTR("Settings.GroupSystem")))
    {
        drawCategoryItem(EUnifiedSettingsCategory::General, ICON_FA_GEAR, "Settings.CategoryGeneral");
        drawCategoryItem(EUnifiedSettingsCategory::Theme, ICON_FA_PALETTE, "Settings.CategoryTheme");
        drawCategoryItem(EUnifiedSettingsCategory::Graphic, ICON_FA_DESKTOP, "Settings.CategoryGraphic");
        ImGui::TreePop();
    }

    // 2. 에디터 및 작업 공간 그룹
    if (ImGui::TreeNodeEx("SidebarGroupEditor", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", LOCAL_CSTR("Settings.GroupEditor")))
    {
        drawCategoryItem(EUnifiedSettingsCategory::Editor, ICON_FA_SLIDERS, "Settings.CategoryEditor");
        drawCategoryItem(EUnifiedSettingsCategory::Workspace, ICON_FA_TABLE_CELLS, "Settings.CategoryWorkspace");
        drawCategoryItem(EUnifiedSettingsCategory::Shortcuts, ICON_FA_KEYBOARD, "Settings.CategoryShortcuts");
        ImGui::TreePop();
    }

    // 3. 프로젝트 그룹
    if (ImGui::TreeNodeEx("SidebarGroupProject", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", LOCAL_CSTR("Settings.GroupProject")))
    {
        drawCategoryItem(EUnifiedSettingsCategory::ProjectSettings, ICON_FA_FOLDER_TREE, "Settings.CategoryProjectSettings");
        ImGui::TreePop();
    }
}

void UnifiedSettingsPanel::DrawContentArea()
{
    switch (m_ActiveCategory)
    {
    case EUnifiedSettingsCategory::General:         DrawCategoryGeneral(); break;
    case EUnifiedSettingsCategory::Theme:           DrawCategoryTheme(); break;
    case EUnifiedSettingsCategory::Graphic:         DrawCategoryGraphic(); break;
    case EUnifiedSettingsCategory::Editor:          DrawCategoryEditor(); break;
    case EUnifiedSettingsCategory::Workspace:       DrawCategoryWorkspace(); break;
    case EUnifiedSettingsCategory::Shortcuts:       DrawCategoryShortcuts(); break;
    case EUnifiedSettingsCategory::ProjectSettings: DrawCategoryProjectSettings(); break;
    default: break;
    }
}

void UnifiedSettingsPanel::DrawFooter()
{
    ImGui::Separator();
    ImGui::Spacing();
    const bool hasChanges = IsAnyDirty();
    auto& loc = Engine::LocalizationManager::Get();
    const std::string textReset = loc.GetText("Settings.Btn.ResetDefaults");
    const std::string textDiscard = loc.GetText("Settings.Btn.Discard");
    const std::string textApply = loc.GetText("Settings.Btn.Apply");
    // 폰트 크기 및 언어에 따른 동적 버튼 너비 산출
    const float paddingX = ImGui::GetStyle().FramePadding.x * 2.0f + 16.0f;
    const float btnResetW = std::max(90.0f, ImGui::CalcTextSize(textReset.c_str()).x + paddingX);
    const float btnDiscardW = std::max(90.0f, ImGui::CalcTextSize(textDiscard.c_str()).x + paddingX);
    const float btnApplyW = std::max(90.0f, ImGui::CalcTextSize(textApply.c_str()).x + paddingX);
    const float btnH = ImGui::GetFrameHeight();
    if (ImGui::Button(textReset.c_str(), ImVec2(btnResetW, btnH)))
    {
        ResetToDefaults();
    }
    const float rightGroupWidth = btnDiscardW + ImGui::GetStyle().ItemSpacing.x + btnApplyW;
    const float availWidth = ImGui::GetContentRegionAvail().x;
    if (availWidth > rightGroupWidth)
    {
        ImGui::SameLine(ImGui::GetCursorPosX() + availWidth - rightGroupWidth);
    }
    else
    {
        ImGui::SameLine();
    }
    ImGui::BeginDisabled(!hasChanges);
    if (ImGui::Button(textDiscard.c_str(), ImVec2(btnDiscardW, btnH)))
    {
        ResetDrafts(); // 저장된 기존 설정으로 초안 복원
    }
    ImGui::SameLine();
    if (ImGui::Button(textApply.c_str(), ImVec2(btnApplyW, btnH)))
    {
        ApplySettings(); // 모든 변경 사항 적용 및 저장
    }
    ImGui::EndDisabled();
}

void UnifiedSettingsPanel::DrawWindowConfirmation()
{
    auto& settings = EditorSettingsService::Get();
    const auto title = LOCAL("EditorSettings.WindowConfirmTitle") + "###EditorWindowConfirmation";

    if (settings.IsAwaitingWindowConfirmation() && !ImGui::IsPopupOpen(title.c_str()))
        ImGui::OpenPopup(title.c_str());

    ImGui::SetNextWindowSizeConstraints(ImVec2(420.0f, 0.0f), ImVec2(600.0f, FLT_MAX));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 16.0f));

    const auto flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, flags))
    {
        ImGui::PopStyleVar();
        return;
    }

    if (!settings.IsAwaitingWindowConfirmation())
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        ImGui::PopStyleVar();
        return;
    }

    ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), ICON_FA_DESKTOP);
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(LOCAL_CSTR("EditorSettings.WindowConfirmTitle"));
    ImGui::Separator();
    ImGui::Spacing();

    const int32 remainingSec = settings.GetWindowConfirmationSeconds();
    ImGui::TextWrapped("%s", LOCAL_FMT("EditorSettings.WindowConfirmMessage", remainingSec).c_str());
    ImGui::Spacing();

    const float progress = static_cast<float>(remainingSec) / 15.0f;
    ImGui::ProgressBar(progress, ImVec2(-1.0f, 5.0f), "");

    ImGui::Spacing();
    ImGui::Spacing();

    const float btnWidth = 110.0f;
    const float btnHeight = 28.0f;
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float totalBtnsWidth = (btnWidth * 2) + spacing;
    const float alignX = ImGui::GetWindowContentRegionMax().x - totalBtnsWidth;

    if (alignX > ImGui::GetCursorPosX())
        ImGui::SameLine(alignX);

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.75f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.55f, 0.88f, 1.0f));
    if (ImGui::Button((std::string(ICON_FA_CHECK " ") + LOCAL("EditorSettings.KeepWindowChange")).c_str(), ImVec2(btnWidth, btnHeight)))
    {
        const auto result = settings.ConfirmWindowChange();
        if (result == EResult::Success || !settings.IsAwaitingWindowConfirmation())
            ImGui::CloseCurrentPopup();
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();

    if (ImGui::Button((std::string(ICON_FA_XMARK " ") + LOCAL("EditorCommon.Cancel")).c_str(), ImVec2(btnWidth, btnHeight)))
    {
        if (settings.CancelWindowChange() == EResult::Success)
            ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    ImGui::PopStyleVar();
}

void UnifiedSettingsPanel::DrawCategoryGeneral()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryGeneral"));
    ImGui::Separator();
    ImGui::Spacing();
    // 1. 언어 및 로캘
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionLanguage"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        const char* languages[] = { "한국어 (Korean)", "English" };
        int currentLangIdx = (m_DraftLanguage == ELocalizationLanguage::English) ? 1 : 0;
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::Combo(LOCAL_LABEL("UI_LANGUAGE").c_str(), &currentLangIdx, languages, IM_ARRAYSIZE(languages)))
        {
            m_DraftLanguage = (currentLangIdx == 1) ? ELocalizationLanguage::English : ELocalizationLanguage::Korean;
            m_LanguageDirty = true;
        }
        ImGui::Spacing();
    }
    // 2. 외형 및 디스플레이 배율 (오직 UI 배율만 전담)
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionAppearance"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat(LOCAL_LABEL("ToolBar.UIScale").c_str(), &m_DraftUIScale, EditorPreferences::MinUIScale, EditorPreferences::MaxUIScale, "%.2fx"))
            m_UIScaleDirty = true;
        ImGui::Spacing();
    }
    // 3. 폰트 및 타이포그래피 (독립 헤더)
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionFont"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::TextUnformatted(LOCAL_CSTR("Settings.CustomFont"));
        ImGui::TextDisabled("%s", m_DraftCustomFontPath.empty() ? "System Default (malgun.ttf)" : m_DraftCustomFontPath.c_str());
        if (ImGui::Button(LOCAL_LABEL("Settings.FontBrowse").c_str()))
        {
            std::wstring selectedPath;
            if (FileDialogs::OpenFileDialog(selectedPath, { { L"Font Files (*.ttf;*.otf)", L"*.ttf;*.otf" } }))
            {
                m_DraftCustomFontPath = WStrToStr(selectedPath);
                m_FontDirty = true;
            }
        }
        if (!m_DraftCustomFontPath.empty())
        {
            ImGui::SameLine();
            if (ImGui::Button(LOCAL_LABEL("Settings.FontReset").c_str()))
            {
                m_DraftCustomFontPath.clear();
                m_FontDirty = true;
            }
        }
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat(LOCAL_LABEL("Settings.FontSize").c_str(), &m_DraftFontSize, 12.0f, 32.0f, "%.0f px"))
            m_FontDirty = true;
        ImGui::Spacing();
    }
    // 4. 자동 저장
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionAutoSave"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        if (ImGui::Checkbox(LOCAL_LABEL("Settings.AutoSaveEnabled").c_str(), &m_DraftAutoSaveEnabled))
            m_EditorDirty = true;
        if (m_DraftAutoSaveEnabled)
        {
            ImGui::SetNextItemWidth(200.0f);
            if (ImGui::SliderInt(LOCAL_LABEL("Settings.AutoSaveInterval").c_str(), &m_DraftAutoSaveIntervalMinutes, 1, 60, "%d min"))
                m_EditorDirty = true;
        }
        ImGui::Spacing();
    }
}
void UnifiedSettingsPanel::DrawCategoryTheme()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryTheme"));
    ImGui::Separator();
    ImGui::Spacing();
    // 1. 테마 프리셋
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.Theme"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        const char* themes[] = {
            "BamEngine Dark (Default)",
            "Light",
            "Classic",
            "Custom (User Saved)"
        };
        int currentThemeIdx = 0;
        if (m_DraftTheme == "Light") currentThemeIdx = 1;
        else if (m_DraftTheme == "Classic") currentThemeIdx = 2;
        else if (m_DraftTheme == "Custom") currentThemeIdx = 3;
        ImGui::SetNextItemWidth(250.0f);
        if (ImGui::Combo(LOCAL_LABEL("Settings.Theme").c_str(), &currentThemeIdx, themes, IM_ARRAYSIZE(themes)))
        {
            if (currentThemeIdx == 1) m_DraftTheme = "Light";
            else if (currentThemeIdx == 2) m_DraftTheme = "Classic";
            else if (currentThemeIdx == 3) m_DraftTheme = "Custom";
            else m_DraftTheme = "Dark";
            ImGuiManager::Get().ApplyTheme(m_DraftTheme);
            m_EditorDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(LOCAL_LABEL("Settings.SaveCustomStyle").c_str()))
        {
            const auto stylePath = ImGuiManager::Get().GetCustomStylePath();
            if (ImGuiManager::Get().SaveCustomStyle(stylePath))
            {
                m_DraftTheme = "Custom";
                m_EditorDirty = true;
                NotificationManager::Get().AddNotification(LOCAL("Settings.CategoryTheme"), LOCAL("Settings.StyleSavedNotification"), ELogLevel::Info);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(LOCAL_LABEL("Settings.ResetCustomStyle").c_str()))
        {
            m_DraftTheme = "Dark";
            ImGuiManager::Get().ApplyDefaultDarkStyle();
            m_EditorDirty = true;
        }
        ImGui::Spacing();
    }
    // 2. ImGui 스타일 상세 커스터마이저
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.StyleEditor"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Modify colors, roundings, and paddings directly. Click [Save Custom Style] or [Apply] to persist.");
        ImGui::Spacing();
        ImGui::ShowStyleEditor(&ImGui::GetStyle());
        m_DraftTheme = "Custom";
        m_EditorDirty = true;
    }
}
void UnifiedSettingsPanel::DrawCategoryGraphic()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryGraphic"));
    ImGui::Separator();
    ImGui::Spacing();
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionDisplayResolution"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt(LOCAL_LABEL("EditorCommon.Width").c_str(), &m_DraftWindowWidth, 50, 100))
            m_WindowDirty = true;
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt(LOCAL_LABEL("EditorCommon.Height").c_str(), &m_DraftWindowHeight, 50, 100))
            m_WindowDirty = true;
        if (ImGui::Checkbox(LOCAL_LABEL("ToolBar.Fullscreen").c_str(), &m_DraftFullscreen))
            m_WindowDirty = true;
        ImGui::Spacing();
    }
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionFrameSync"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        const int fpsOptions[] = { 0, 30, 60, 120, 144, 240 };
        const char* fpsLabels[] = { "Unlimited", "30 FPS", "60 FPS", "120 FPS", "144 FPS", "240 FPS" };
        int currentFpsIdx = 0;
        for (int i = 0; i < IM_ARRAYSIZE(fpsOptions); ++i)
        {
            if (m_DraftTargetFPS == fpsOptions[i]) { currentFpsIdx = i; break; }
        }
        ImGui::SetNextItemWidth(150.0f);
        if (ImGui::Combo(LOCAL_LABEL("ToolBar.TargetFPS").c_str(), &currentFpsIdx, fpsLabels, IM_ARRAYSIZE(fpsLabels)))
        {
            m_DraftTargetFPS = fpsOptions[currentFpsIdx];
            m_TargetFPSDirty = true;
        }
        if (ImGui::Checkbox(LOCAL_LABEL("GraphicsSettings.VSync").c_str(), &m_DraftVSync))
            m_VSyncDirty = true;
        ImGui::Spacing();
    }
}

void UnifiedSettingsPanel::HandleKeyCapture()
{
    if (m_ListeningActionId.empty()) return;

    auto& shortcuts = ShortcutManager::Get();
    auto& input = Engine::InputManager::Get();

    // Escape 누르면 바인딩 취소
    if (input.IsKeyButtonDown(Engine::EKeyCode::Escape))
    {
        m_ListeningActionId.clear();
        m_ConflictWarning.clear();
        shortcuts.SetCapturing(false);
        return;
    }

    // Backspace 누르면 단축키 제거 (Unbound)
    if (input.IsKeyButtonDown(Engine::EKeyCode::Backspace))
    {
        shortcuts.Rebind(m_ListeningActionId, ShortcutChord());
        shortcuts.SaveToFile();
        m_ListeningActionId.clear();
        m_ConflictWarning.clear();
        shortcuts.SetCapturing(false);
        shortcuts.SuppressNextProcess();
        return;
    }

    uint8 modifiers = 0;
    if (input.IsCtrlPressed())  modifiers |= static_cast<uint8>(EModifierKey::Ctrl);
    if (input.IsShiftPressed()) modifiers |= static_cast<uint8>(EModifierKey::Shift);
    if (input.IsAltPressed())   modifiers |= static_cast<uint8>(EModifierKey::Alt);

    for (uint32 k = 1; k < static_cast<uint32>(Engine::EKeyCode::Count); ++k)
    {
        const auto key = static_cast<Engine::EKeyCode>(k);
        if (key == Engine::EKeyCode::LCtrl || key == Engine::EKeyCode::RCtrl ||
            key == Engine::EKeyCode::LShift || key == Engine::EKeyCode::RShift ||
            key == Engine::EKeyCode::LAlt || key == Engine::EKeyCode::RAlt)
        {
            continue;
        }

        if (input.IsKeyButtonDown(key))
        {
            const ShortcutChord newChord(key, modifiers);
            const auto* action = shortcuts.FindAction(m_ListeningActionId);

            if (action)
            {
                const std::string conflictId = shortcuts.FindConflict(newChord, action->Context, m_ListeningActionId);
                if (!conflictId.empty())
                {
                    const auto* conflictAction = shortcuts.FindAction(conflictId);
                    const std::string conflictName = (conflictAction && !conflictAction->DisplayNameKey.empty()) ? LOCAL(conflictAction->DisplayNameKey) : conflictId;
                    m_ConflictWarning = LOCAL_FMT("Shortcuts.ConflictWarning", newChord.ToString(), conflictName);
                }
                else
                {
                    m_ConflictWarning.clear();
                }

                // 충돌하는 이전 액션을 해제하며 새 단축키로 Rebind
                shortcuts.Rebind(m_ListeningActionId, newChord, true);
                shortcuts.SaveToFile();
            }

            m_ListeningActionId.clear();
            shortcuts.SetCapturing(false);
            shortcuts.SuppressNextProcess(); // 이번 프레임의 단축키 발동 차단
            break;
        }
    }
}

void UnifiedSettingsPanel::DrawCategoryShortcuts()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryShortcuts"));
    ImGui::Separator();
    ImGui::Spacing();

    HandleKeyCapture();

    if (ImGui::Button(LOCAL_CSTR("Shortcuts.ResetAll")))
    {
        ShortcutManager::Get().ResetAllBindings();
        ShortcutManager::Get().SaveToFile();
        m_ConflictWarning.clear();
    }

    if (!m_ConflictWarning.empty())
    {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_ConflictWarning.c_str());
    }

    ImGui::Spacing();

    auto& shortcuts = ShortcutManager::Get();
    const auto& actions = shortcuts.GetActions();

    // 1. 카테고리별 액션 분류
    std::map<std::string, std::vector<std::string>> categorizedActions;
    for (const auto& [id, action] : actions)
    {
        const std::string catKey = action.CategoryKey.empty() ? "UI_GENERAL" : action.CategoryKey;
        categorizedActions[catKey].push_back(id);
    }

    constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit;

    // 2. 카테고리별 CollapsingHeader 렌더링
    for (const auto& [catKey, actionIds] : categorizedActions)
    {
        std::vector<std::string> visibleIds;
        for (const auto& id : actionIds)
        {
            const auto it = actions.find(id);
            if (it == actions.end()) continue;
            const auto& action = it->second;

            const std::string displayName = action.DisplayNameKey.empty() ? id : LOCAL(action.DisplayNameKey);
            const std::string category = LOCAL(catKey);
            const ShortcutChord activeChord = shortcuts.GetActiveChord(id);
            const std::string chordText = activeChord.IsValid() ? activeChord.ToString() : LOCAL("EditorCommon.None");

            if (m_SearchFilter.PassFilter(displayName.c_str()) || m_SearchFilter.PassFilter(id.c_str()) ||
                m_SearchFilter.PassFilter(category.c_str()) || m_SearchFilter.PassFilter(chordText.c_str()))
            {
                visibleIds.push_back(id);
            }
        }

        if (visibleIds.empty()) continue;

        const std::string headerTitle = LOCAL(catKey) + " (" + std::to_string(visibleIds.size()) + ")###Header_" + catKey;
        if (ImGui::CollapsingHeader(headerTitle.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            const std::string tableId = "Table_" + catKey;
            if (ImGui::BeginTable(tableId.c_str(), 3, tableFlags))
            {
                ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColAction"), ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColShortcut"), ImGuiTableColumnFlags_WidthFixed, 180.0f);
                ImGui::TableSetupColumn(LOCAL_CSTR("Shortcuts.ColReset"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableHeadersRow();

                for (const auto& id : visibleIds)
                {
                    const auto& action = actions.at(id);
                    const std::string displayName = action.DisplayNameKey.empty() ? id : LOCAL(action.DisplayNameKey);
                    const ShortcutChord activeChord = shortcuts.GetActiveChord(id);
                    const std::string chordText = activeChord.IsValid() ? activeChord.ToString() : LOCAL("EditorCommon.None");

                    ImGui::TableNextRow();

                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(displayName.c_str());
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", id.c_str());

                    ImGui::TableNextColumn();
                    ImGui::PushID(id.c_str());

                    const bool isListening = (m_ListeningActionId == id);
                    const std::string buttonLabel = isListening ? LOCAL("Shortcuts.PressKey") : "[" + chordText + "]";

                    if (isListening) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.85f, 0.5f, 0.1f, 1.0f));

                    if (ImGui::Button(buttonLabel.c_str(), ImVec2(-1.0f, 0.0f)))
                    {
                        m_ListeningActionId = isListening ? "" : id;
                        m_ConflictWarning.clear();
                        ShortcutManager::Get().SetCapturing(!m_ListeningActionId.empty());
                    }

                    if (isListening) ImGui::PopStyleColor();

                    ImGui::TableNextColumn();
                    const bool isModified = (activeChord != action.DefaultChord);
                    if (!isModified) ImGui::BeginDisabled();

                    if (ImGui::Button(LOCAL_CSTR("Shortcuts.Reset")))
                    {
                        shortcuts.ResetBinding(id);
                        shortcuts.SaveToFile();
                        m_ConflictWarning.clear();
                    }

                    if (!isModified) ImGui::EndDisabled();
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
            ImGui::Spacing();
        }
    }
}

void UnifiedSettingsPanel::DrawCategoryEditor()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryEditor"));
    ImGui::Separator();
    ImGui::Spacing();
    // 1. 기즈모 스냅 단위
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.GizmoSnapping"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::DragFloat(LOCAL_LABEL("Viewport.SnapTranslation").c_str(), &m_DraftGizmoSnapTranslation, 0.1f, 0.01f, 100.0f, "%.2f"))
            m_EditorDirty = true;
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::DragFloat(LOCAL_LABEL("Viewport.SnapRotation").c_str(), &m_DraftGizmoSnapRotation, 1.0f, 1.0f, 180.0f, "%.1f deg"))
            m_EditorDirty = true;
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::DragFloat(LOCAL_LABEL("Viewport.SnapScale").c_str(), &m_DraftGizmoSnapScale, 0.05f, 0.01f, 10.0f, "%.2f"))
            m_EditorDirty = true;
        ImGui::Spacing();
    }
    // 2. 뷰포트 탐색 카메라 속도
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.CameraMovement"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat(LOCAL_LABEL("Viewport.CameraSpeed").c_str(), &m_DraftCameraSpeed, 1.0f, 500.0f, "%.1f"))
            m_EditorDirty = true;
        ImGui::Spacing();
    }
    // 3. 최소 콘솔 로그 레벨
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.Logging"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        const char* logLevels[] = { "Trace", "Info", "Warn", "Error", "Fatal" };
        int currentLevelIdx = static_cast<int>(m_DraftLogLevel);
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::Combo("Minimum Log Level", &currentLevelIdx, logLevels, IM_ARRAYSIZE(logLevels)))
        {
            m_DraftLogLevel = static_cast<Engine::ELogLevel>(currentLevelIdx);
            m_EditorDirty = true;
        }
        ImGui::Spacing();
    }
    // 4. 카메라 화각
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.CameraMovement"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat(LOCAL_LABEL("Viewport.CameraSpeed").c_str(), &m_DraftCameraSpeed, 1.0f, 500.0f, "%.1f"))
            m_EditorDirty = true;
        ImGui::SetNextItemWidth(200.0f);
        if (ImGui::SliderFloat(LOCAL_LABEL("Settings.CameraFOV").c_str(), &m_DraftCameraFOV, 30.0f, 120.0f, "%.0f deg"))
            m_EditorDirty = true;
        ImGui::Spacing();
    }
}
void UnifiedSettingsPanel::DrawCategoryWorkspace()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryWorkspace"));
    ImGui::Separator();
    ImGui::Spacing();

    auto& settings = EditorSettingsService::Get();

    // 1. 뷰포트 해상도 설정
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionViewportResolution"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        auto states = settings.GetViewportResolutionStates();

        if (states.empty())
        {
            m_SelectedViewportName.clear();
            ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.NoViewport"));
        }
        else
        {
            auto selected = std::find_if(states.begin(), states.end(), [this](const auto& snapshot) {
                return snapshot.Name == m_SelectedViewportName;
                });
            if (selected == states.end()) selected = states.begin();
            m_SelectedViewportName = selected->Name;

            const bool blocked = settings.IsWindowChangePending() || settings.IsVSyncChangePending() || settings.IsViewportResolutionChangePending();
            ImGui::BeginDisabled(blocked);

            // 타깃 뷰포트 선택 콤보
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo(LOCAL_LABEL("GraphicsSettings.TargetViewport").c_str(), selected->DisplayName.c_str()))
            {
                for (auto current = states.begin(); current != states.end(); ++current)
                {
                    const bool isSelected = (current->Name == m_SelectedViewportName);
                    const auto label = current->DisplayName + "###ResolutionTarget." + WStrToStr(current->Name);
                    if (ImGui::Selectable(label.c_str(), isSelected))
                    {
                        m_SelectedViewportName = current->Name;
                        selected = current;
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            const auto& actual = selected->State;
            const auto found = m_ViewportDrafts.find(selected->Name);
            auto draft = found != m_ViewportDrafts.end() ? found->second : actual;
            bool changed = false;

            static constexpr const char* modeKeys[] = { "Viewport.FreePanelSize", "GraphicsSettings.ViewportPreset", "GraphicsSettings.ViewportCustom" };
            const std::array<std::string, 7> aspects = { LOCAL("Viewport.FreeAspect"), "16:9", "16:10", "4:3", "21:9", "1:1", "9:16" };

            // 해상도 모드 콤보
            const auto draftMode = glm::clamp(draft.ResolutionMode, 0, 2);
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo(LOCAL_LABEL("GraphicsSettings.ViewportMode").c_str(), LOCAL_CSTR(modeKeys[draftMode])))
            {
                for (int32 index = 0; index < 3; ++index)
                {
                    if (ImGui::Selectable(LOCAL_LABEL(modeKeys[index]).c_str(), draft.ResolutionMode == index))
                    {
                        draft.ResolutionMode = index;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            // 프리셋 선택
            if (draft.ResolutionMode == static_cast<int32>(EViewportResolutionMode::Preset))
            {
                std::string preview = LOCAL("GraphicsSettings.ViewportPreset");
                for (const auto& preset : g_ResolutionPresets)
                {
                    if (draft.PresetWidth == static_cast<int32>(preset.Width) && draft.PresetHeight == static_cast<int32>(preset.Height))
                    {
                        preview = LOCAL(preset.NameKey);
                        break;
                    }
                }

                ImGui::SetNextItemWidth(220.0f);
                if (ImGui::BeginCombo(LOCAL_LABEL("GraphicsSettings.ViewportPreset").c_str(), preview.c_str()))
                {
                    for (const auto& preset : g_ResolutionPresets)
                    {
                        const bool isSelected = (draft.PresetWidth == static_cast<int32>(preset.Width) && draft.PresetHeight == static_cast<int32>(preset.Height));
                        if (ImGui::Selectable(LOCAL_LABEL(preset.NameKey).c_str(), isSelected))
                        {
                            draft.PresetWidth = static_cast<int32>(preset.Width);
                            draft.PresetHeight = static_cast<int32>(preset.Height);
                            changed = true;
                        }
                    }
                    ImGui::EndCombo();
                }
            }
            else if (draft.ResolutionMode == static_cast<int32>(EViewportResolutionMode::Custom))
            {
                ImGui::SetNextItemWidth(120.0f);
                bool customChanged = ImGui::InputInt(LOCAL_LABEL("EditorCommon.Width").c_str(), &draft.CustomWidth, 50, 100);
                ImGui::SetNextItemWidth(120.0f);
                customChanged |= ImGui::InputInt(LOCAL_LABEL("EditorCommon.Height").c_str(), &draft.CustomHeight, 50, 100);

                if (customChanged)
                {
                    draft.CustomWidth = glm::clamp(draft.CustomWidth, BaseViewportPanel::MinResolutionDimension, BaseViewportPanel::MaxResolutionWidth);
                    draft.CustomHeight = glm::clamp(draft.CustomHeight, BaseViewportPanel::MinResolutionDimension, BaseViewportPanel::MaxResolutionHeight);
                    changed = true;
                }
            }

            // 종횡비 콤보
            const auto draftAspect = glm::clamp(draft.AspectRatioMode, 0, 6);
            ImGui::SetNextItemWidth(220.0f);
            if (ImGui::BeginCombo(LOCAL_LABEL("Viewport.AspectRatio").c_str(), aspects[draftAspect].c_str()))
            {
                for (int32 index = 0; index < static_cast<int32>(aspects.size()); ++index)
                {
                    const auto label = aspects[index] + "###ViewportAspect." + std::to_string(index);
                    if (ImGui::Selectable(label.c_str(), draft.AspectRatioMode == index))
                    {
                        draft.AspectRatioMode = index;
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }

            if (changed)
                m_ViewportDrafts.insert_or_assign(selected->Name, draft);

            ImGui::Spacing();
            if (ImGui::Button(LOCAL_LABEL("GraphicsSettings.ApplyViewport").c_str()))
            {
                if (settings.RequestViewportResolutionChange(selected->Name, draft) != EResult::Success)
                    NotificationManager::Get().AddNotification(LOCAL("GraphicsSettings.Title"), LOCAL("GraphicsSettings.ViewportRequestRejected"), ELogLevel::Warn);
            }
            ImGui::SameLine();
            if (ImGui::Button(LOCAL_LABEL("GraphicsSettings.ReloadViewport").c_str()))
                m_ViewportDrafts.erase(selected->Name);

            ImGui::EndDisabled();

            if (settings.IsViewportResolutionChangePending())
                ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "%s", LOCAL_CSTR("GraphicsSettings.ViewportPending"));
        }
        ImGui::Spacing();
    }

    // 2. 콘텐츠 브라우저 환경설정
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionContentBrowser"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::SetNextItemWidth(220.0f);
        if (ImGui::SliderFloat(LOCAL_LABEL("Settings.ThumbnailSize").c_str(), &m_DraftThumbnailSize, 32.0f, 128.0f, "%.0f px"))
            m_WorkspaceDirty = true;
        ImGui::Spacing();
    }

    // 3. 도킹 및 레이아웃
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionDockingLayout"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Default Docking Layout state is tracked in Workspace.json.");
        ImGui::Spacing();
    }
}

void UnifiedSettingsPanel::DrawCategoryProjectSettings()
{
    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryProjectSettings"));
    ImGui::Separator();
    ImGui::Spacing();
    auto& settings = EditorSettingsService::Get();
    const auto& projectRoot = settings.GetProjectRoot();
    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.ProjectMetadata"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::TextDisabled("%s: %s", LOCAL_CSTR("Settings.ProjectRoot"), projectRoot.string().c_str());
        ImGui::Spacing();
        // 1. 프로젝트 이름
        char nameBuf[256];
        strncpy_s(nameBuf, m_DraftProjectName.c_str(), sizeof(nameBuf));
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::InputText(LOCAL_LABEL("Settings.ProjectName").c_str(), nameBuf, sizeof(nameBuf)))
        {
            m_DraftProjectName = nameBuf;
            m_ProjectSettingsDirty = true;
        }
        // 2. 시작 씬 경로 (파일 다이얼로그 탐색기)
        char sceneBuf[512];
        strncpy_s(sceneBuf, m_DraftStartupScenePath.c_str(), sizeof(sceneBuf));
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::InputText(LOCAL_LABEL("Settings.StartupScene").c_str(), sceneBuf, sizeof(sceneBuf)))
        {
            m_DraftStartupScenePath = sceneBuf;
            m_ProjectSettingsDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button((LOCAL("Settings.BrowseScene") + "###BrowseStartupScene").c_str()))
        {
            std::wstring scenePath;
            const auto scenesDir = projectRoot / "Resources" / "Scenes";
            if (FileDialogs::OpenFileDialog(scenePath, { { L"Scene Files (*.bamscene.json;*.json)", L"*.bamscene.json;*.json" } }, scenesDir.wstring()))
            {
                std::error_code ec;
                const auto relPath = std::filesystem::relative(scenePath, projectRoot, ec);
                m_DraftStartupScenePath = ec ? WStrToStr(scenePath) : relPath.generic_string();
                m_ProjectSettingsDirty = true;
            }
        }
        // 3. 에셋 폴더 경로 (폴더 다이얼로그 탐색기)
        char assetBuf[256];
        strncpy_s(assetBuf, m_DraftAssetDirectory.c_str(), sizeof(assetBuf));
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::InputText(LOCAL_LABEL("Settings.AssetDirectory").c_str(), assetBuf, sizeof(assetBuf)))
        {
            m_DraftAssetDirectory = assetBuf;
            m_ProjectSettingsDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button((LOCAL("Settings.BrowseFolder") + "###BrowseAssetDir").c_str()))
        {
            std::wstring folderPath;
            if (FileDialogs::OpenFolderDialog(folderPath, projectRoot.wstring()))
            {
                std::error_code ec;
                const auto relPath = std::filesystem::relative(folderPath, projectRoot, ec);
                m_DraftAssetDirectory = ec ? WStrToStr(folderPath) : relPath.generic_string();
                m_ProjectSettingsDirty = true;
            }
        }
        // 4. 셰이더 소스 경로 (폴더 다이얼로그 탐색기)
        char shaderBuf[256];
        strncpy_s(shaderBuf, m_DraftShaderSourceDirectory.c_str(), sizeof(shaderBuf));
        ImGui::SetNextItemWidth(300.0f);
        if (ImGui::InputText(LOCAL_LABEL("Settings.ShaderDirectory").c_str(), shaderBuf, sizeof(shaderBuf)))
        {
            m_DraftShaderSourceDirectory = shaderBuf;
            m_ProjectSettingsDirty = true;
        }
        ImGui::SameLine();
        if (ImGui::Button((LOCAL("Settings.BrowseFolder") + "###BrowseShaderDir").c_str()))
        {
            std::wstring folderPath;
            if (FileDialogs::OpenFolderDialog(folderPath, projectRoot.wstring()))
            {
                std::error_code ec;
                const auto relPath = std::filesystem::relative(folderPath, projectRoot, ec);
                m_DraftShaderSourceDirectory = ec ? WStrToStr(folderPath) : relPath.generic_string();
                m_ProjectSettingsDirty = true;
            }
        }
        ImGui::Spacing();
    }
}

END
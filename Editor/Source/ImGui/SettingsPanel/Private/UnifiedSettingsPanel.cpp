#include "UnifiedSettingsPanel.h"
#include "EditorSettingsService.h"
#include "ImGuiManager.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"

UnifiedSettingsPanel::UnifiedSettingsPanel()
{
    m_Name = L"UnifiedSettings";
    m_DisplayNameKey = "Settings.Title";
    m_Open = false;
}

bool UnifiedSettingsPanel::IsAnyDirty() const
{
    return m_General.IsDirty() || m_Theme.IsDirty() || m_Graphics.IsDirty() ||
        m_EditorEnvironment.IsDirty() || m_Workspace.IsDirty() ||
        m_ProjectSettings.IsDirty() || m_InputActions.IsDirty();
}

void UnifiedSettingsPanel::Update(f32)
{
    auto& settings = EditorSettingsService::Get();

    if (m_StartupSettingsNoticePending)
    {
        m_StartupSettingsNoticePending = false;

        if (settings.IsPreferencesSaveBlocked())
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"),
                LOCAL("EditorSettings.PreferencesStartupFallback"), ELogLevel::Warn);
        }

        if (settings.IsWorkspaceSaveBlocked())
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorSettings.Title"),
                LOCAL("EditorSettings.WorkspaceStartupFallback"), ELogLevel::Warn);
        }
    }

    m_Graphics.Update(settings);
    m_Workspace.Update(settings);
}

void UnifiedSettingsPanel::ResetDrafts()
{
    auto& settings = EditorSettingsService::Get();
    const auto& preferences = settings.GetPreferences();

    m_General.ReloadDraft(preferences);
    m_Theme.ReloadDraft(preferences);
    m_Graphics.ReloadDraft(settings);
    m_EditorEnvironment.ReloadDraft(preferences);
    m_Workspace.ReloadDraft(settings);
    m_ProjectSettings.ReloadDraft(settings);
    m_InputActions.ReloadDraft(settings);

    m_DraftsInitialized = true;
}

void UnifiedSettingsPanel::ResetToDefaults()
{
    m_InputActions.CancelCapture();

    const EditorPreferences defaults;
    m_General.ResetToDefaults(defaults);
    m_Theme.ResetToDefaults(defaults);
    m_EditorEnvironment.ResetToDefaults(defaults);
}

void UnifiedSettingsPanel::ApplySettings()
{
    m_InputActions.CancelCapture();
    auto& settings = EditorSettingsService::Get();

    if (m_General.IsDirty() || m_Theme.IsDirty() || m_EditorEnvironment.IsDirty())
    {
        auto candidate = settings.GetPreferences();
        m_General.WriteDraft(candidate);
        m_Theme.WriteDraft(candidate);
        m_EditorEnvironment.WriteDraft(candidate);

        const auto result = settings.SetEditorPreferences(candidate);
        ImGuiManager::Get().NotifySettingsChange(result);
        m_EditorEnvironment.FinishApply(result.Applied);
        m_General.FinishApply(result.Applied);
        m_Theme.FinishApply();
    }

    m_Graphics.Apply(settings);
    m_Workspace.SaveWorkspace(settings);
    m_ProjectSettings.Apply(settings);
    m_Workspace.ApplyViewportDrafts(settings);
    m_InputActions.Apply(settings);
}

void UnifiedSettingsPanel::Draw()
{
    m_Graphics.DrawWindowConfirmation(EditorSettingsService::Get());

    if (!m_Open)
    {
        m_DraftsInitialized = false;
        m_Shortcuts.CancelCapture();
        m_InputActions.CancelCapture();
        return;
    }

    if (!m_DraftsInitialized)
        ResetDrafts();

    if (m_ActiveCategory == EUnifiedSettingsCategory::Shortcuts)
        m_Shortcuts.HandleCapture();

    ImGui::SetNextWindowSize(ImVec2(860.0f, 580.0f), ImGuiCond_FirstUseEver);
    const std::string windowTitle = LOCAL("Settings.Title") + "###UnifiedSettingsWindow";

    const ImGuiWindowFlags windowFlags = IsAnyDirty() ? ImGuiWindowFlags_UnsavedDocument : ImGuiWindowFlags_None;


    if (!BeginPanelWindow(windowTitle.c_str(), m_Open, windowFlags) || !m_Open)
    {
        m_InputActions.CancelCapture();
        ImGui::End();
        return;
    }

    m_SearchFilter.Draw(LOCAL_CSTR("Shortcuts.Search"), 300.0f);
    ImGui::Separator();

    const float footerHeight = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    const float contentHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y - footerHeight);

    Editor::DrawSidebarLayout("SettingsSidebarRegion", "##SidebarSplitter", "SettingsContentRegion",
        m_SidebarWidth, contentHeight, 180.0f, 200.0f,
        [this] { DrawSidebar(); },
        [this]
        {
            DrawSettingsStorageStatus();
            DrawContentArea();
        });

    ImGui::Separator();
    DrawFooter();
    m_InputActions.HandleCapture(m_Open && m_ActiveCategory == EUnifiedSettingsCategory::InputActions);
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
                m_Shortcuts.CancelCapture();
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
        drawCategoryItem(EUnifiedSettingsCategory::InputActions, ICON_FA_KEYBOARD, "InputActions.Title");
        ImGui::TreePop();
    }
}

void UnifiedSettingsPanel::DrawContentArea()
{
    switch (m_ActiveCategory)
    {
    case EUnifiedSettingsCategory::General: m_General.Draw(); break;
    case EUnifiedSettingsCategory::Theme: m_Theme.Draw(); break;
    case EUnifiedSettingsCategory::Graphic: m_Graphics.Draw(); break;
    case EUnifiedSettingsCategory::Editor: m_EditorEnvironment.Draw(); break;
    case EUnifiedSettingsCategory::Workspace: m_Workspace.Draw(EditorSettingsService::Get()); break;
    case EUnifiedSettingsCategory::Shortcuts: m_Shortcuts.Draw(m_SearchFilter); break;
    case EUnifiedSettingsCategory::ProjectSettings: m_ProjectSettings.Draw(EditorSettingsService::Get()); break;
    case EUnifiedSettingsCategory::InputActions: m_InputActions.Draw(); break;
    default: break;
    }
}

void UnifiedSettingsPanel::DrawSettingsStorageStatus() const
{
    const auto& settings = EditorSettingsService::Get();
    const bool preferencesBlocked = settings.IsPreferencesSaveBlocked();
    const bool workspaceBlocked = settings.IsWorkspaceSaveBlocked();

    if (!preferencesBlocked && !workspaceBlocked)
        return;

    const auto drawStatus = [](const char* messageKey, const std::filesystem::path& path)
        {
            ImGui::TextWrapped("%s", LOCAL_CSTR(messageKey));

            std::string pathText = LOCAL("EditorSettings.PathUnavailable");
            if (!path.empty())
            {
                const auto utf8 = path.generic_u8string();
                pathText.assign(utf8.begin(), utf8.end());
            }

            ImGui::TextWrapped("%s", LOCAL_FMT("EditorSettings.SettingsFilePath", pathText).c_str());
        };

    if (preferencesBlocked)
        drawStatus("EditorSettings.PreferencesSaveBlocked", settings.GetPreferencesPath());

    if (workspaceBlocked)
        drawStatus("EditorSettings.WorkspaceSaveBlocked", settings.GetWorkspacePath());

    ImGui::TextWrapped("%s", LOCAL_CSTR("EditorSettings.SettingsRecoveryHint"));
    ImGui::Separator();
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

void UnifiedSettingsPanel::CaptureState(EditorWorkspace& workspace) const
{
    IImGui::CaptureState(workspace);
    workspace.SetSplitter("Settings.SidebarWidth", m_SidebarWidth);
}

void UnifiedSettingsPanel::RestoreState(const EditorWorkspace& workspace)
{
    IImGui::RestoreState(workspace);
    m_SidebarWidth = workspace.GetSplitter("Settings.SidebarWidth", 250.0f);

    if (workspace.PanelOpenStates.contains(GetName()))
        return;

    const auto legacy = workspace.PanelOpenStates.find(L"GraphicsSettings");
    const bool wasGraphicsOpen = legacy != workspace.PanelOpenStates.end()
        ? legacy->second : workspace.DisplaySettingsOpen;

    if (wasGraphicsOpen)
        OpenWithCategory(EUnifiedSettingsCategory::Graphic);
}
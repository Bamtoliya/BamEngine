#include "GraphicSettingsWidget.h"
#include "EditorUI.h"
#include "Application.h"
#include "EditorSettingsService.h"
#include "ImGuiManager.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"
#include "TimeManager.h"

void GraphicSettingsWidget::ReloadDraft(const EditorSettingsService& settings)
{
    auto& application = Application::Get();
    application.GetWindowSize(&m_DraftWindowWidth, &m_DraftWindowHeight);
    m_DraftFullscreen = application.IsFullscreen();
    m_DraftTargetFPS = static_cast<std::int32_t>(Engine::TimeManager::Get().GetTargetFPS());
    m_DraftVSync = settings.GetEditorVSync().value_or(settings.GetPreferences().VSync);

    m_WindowDirty = false;
    m_TargetFPSDirty = false;
    m_VSyncDirty = false;
}

void GraphicSettingsWidget::Apply(EditorSettingsService& settings)
{
    if (m_TargetFPSDirty)
    {
        const auto result = settings.SetEditorTargetFPS(m_DraftTargetFPS);
        ImGuiManager::Get().NotifySettingsChange(result);
        if (result.Applied)
            m_TargetFPSDirty = false;
    }

    if (m_WindowDirty && !settings.IsWindowChangePending() && !settings.IsVSyncChangePending())
    {
        const EditorWindowRequest request{ m_DraftWindowWidth, m_DraftWindowHeight, m_DraftFullscreen };
        if (request.HasValidValues() && settings.RequestWindowChange(request) != EResult::Success)
        {
            NotificationManager::Get().AddNotification(
                LOCAL("EditorSettings.Title"), LOCAL("EditorSettings.WindowRequestRejected"), ELogLevel::Error);
        }
    }

    if (m_VSyncDirty && !settings.IsWindowChangePending() && !settings.IsVSyncChangePending())
    {
        if (settings.RequestVSyncChange(m_DraftVSync) != EResult::Success)
        {
            NotificationManager::Get().AddNotification(
                LOCAL("GraphicsSettings.Title"), LOCAL("GraphicsSettings.VSyncRequestRejected"), ELogLevel::Warn);
        }
    }
}

void GraphicSettingsWidget::Update(EditorSettingsService& settings)
{
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
}

void GraphicSettingsWidget::Draw()
{
    const auto& settings = EditorSettingsService::Get();
    const bool windowPending = settings.IsWindowChangePending();
    const bool vsyncPending = settings.IsVSyncChangePending();
    const bool changePending = windowPending || vsyncPending || settings.IsViewportResolutionChangePending();

    ImGui::TextUnformatted(LOCAL_CSTR("Settings.CategoryGraphic"));
    ImGui::Separator();
    ImGui::Spacing();

    const EditorUI::PropertyTableStyle tableStyle;

    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionDisplayResolution"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::BeginDisabled(changePending);
        if (EditorUI::BeginPropertyTable("##GraphicDisplayTable", tableStyle))
        {
            // Width
            EditorUI::PropertyRow("EditorCommon.Width");
            ImGui::BeginDisabled(m_DraftFullscreen);
            ImGui::SetNextItemWidth(-FLT_MIN);
            m_WindowDirty |= ImGui::InputInt("###WindowWidth", &m_DraftWindowWidth, 50, 100);
            ImGui::EndDisabled();

            // Height
            EditorUI::PropertyRow("EditorCommon.Height");
            ImGui::BeginDisabled(m_DraftFullscreen);
            ImGui::SetNextItemWidth(-FLT_MIN);
            m_WindowDirty |= ImGui::InputInt("###WindowHeight", &m_DraftWindowHeight, 50, 100);
            ImGui::EndDisabled();

            // Fullscreen
            EditorUI::PropertyRow("ToolBar.Fullscreen");
            m_WindowDirty |= ImGui::Checkbox("###Fullscreen", &m_DraftFullscreen);

            EditorUI::EndPropertyTable();
        }
        ImGui::EndDisabled();

        const EditorWindowRequest request{ m_DraftWindowWidth, m_DraftWindowHeight, m_DraftFullscreen };
        if (!request.HasValidValues())
            ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.InvalidWindowSize"));

        if (windowPending)
            ImGui::TextUnformatted(LOCAL_CSTR("EditorSettings.WindowApplying"));
        else if (settings.IsWindowCaptureBlocked())
            ImGui::TextWrapped("%s", LOCAL_CSTR("EditorSettings.WindowCaptureBlocked"));

        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionFrameSync"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (EditorUI::BeginPropertyTable("##GraphicFrameSyncTable", tableStyle))
        {
            // Target FPS
            EditorUI::PropertyRow("ToolBar.TargetFPS");
            ImGui::SetNextItemWidth(-FLT_MIN);
            m_TargetFPSDirty |= ImGui::DragInt("###TargetFPS", &m_DraftTargetFPS,
                1.0f, 0, EditorPreferences::MaxTargetFPS, "%d", ImGuiSliderFlags_AlwaysClamp);

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", LOCAL_CSTR("ToolBar.TargetFPSHint"));

            // VSync
            const auto actualVSync = settings.GetEditorVSync();
            EditorUI::PropertyRow("GraphicsSettings.VSync");
            ImGui::BeginDisabled(changePending || !actualVSync);
            m_VSyncDirty |= ImGui::Checkbox("###VSync", &m_DraftVSync);
            ImGui::EndDisabled();

            EditorUI::EndPropertyTable();
        }

        const auto actualVSync = settings.GetEditorVSync();
        ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.VSyncHint"));

        if (actualVSync && *actualVSync != m_DraftVSync && !settings.SupportsEditorVSync(m_DraftVSync))
            ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.VSyncUnsupported"));

        if (actualVSync && *actualVSync != settings.GetPreferences().VSync)
            ImGui::TextWrapped("%s", LOCAL_CSTR("GraphicsSettings.VSyncPreferenceMismatch"));

        if (vsyncPending)
            ImGui::TextUnformatted(LOCAL_CSTR("GraphicsSettings.VSyncPending"));

        ImGui::Spacing();
    }
}

void GraphicSettingsWidget::DrawWindowConfirmation(EditorSettingsService& settings)
{
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

    const EditorUI::DialogAction actions[] = {
        { "EditorCommon.Cancel" }, { "EditorSettings.KeepWindowChange" }
    };
    const int action = EditorUI::DialogButtonBar("WindowConfirmationActions", actions, IM_ARRAYSIZE(actions));
    if (action == 1)
    {
        const auto result = settings.ConfirmWindowChange();
        if (result == EResult::Success || !settings.IsAwaitingWindowConfirmation())
            ImGui::CloseCurrentPopup();
    }
    else if (action == 0)
    {
        if (settings.CancelWindowChange() == EResult::Success)
            ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
    ImGui::PopStyleVar();
}
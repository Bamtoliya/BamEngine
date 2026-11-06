#include "GraphicSettingsWidget.h"
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

    if (ImGui::CollapsingHeader(LOCAL_CSTR("Settings.SectionDisplayResolution"), ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Spacing();
        ImGui::BeginDisabled(changePending);
        ImGui::BeginDisabled(m_DraftFullscreen);

        ImGui::SetNextItemWidth(120.0f);
        m_WindowDirty |= ImGui::InputInt(LOCAL_LABEL("EditorCommon.Width").c_str(), &m_DraftWindowWidth, 50, 100);
        ImGui::SetNextItemWidth(120.0f);
        m_WindowDirty |= ImGui::InputInt(LOCAL_LABEL("EditorCommon.Height").c_str(), &m_DraftWindowHeight, 50, 100);

        ImGui::EndDisabled();
        m_WindowDirty |= ImGui::Checkbox(LOCAL_LABEL("ToolBar.Fullscreen").c_str(), &m_DraftFullscreen);
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
        ImGui::Spacing();
        ImGui::SetNextItemWidth(150.0f);
        m_TargetFPSDirty |= ImGui::DragInt(LOCAL_LABEL("ToolBar.TargetFPS").c_str(), &m_DraftTargetFPS,
            1.0f, 0, EditorPreferences::MaxTargetFPS, "%d", ImGuiSliderFlags_AlwaysClamp);

        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", LOCAL_CSTR("ToolBar.TargetFPSHint"));

        const auto actualVSync = settings.GetEditorVSync();
        ImGui::BeginDisabled(changePending || !actualVSync);
        m_VSyncDirty |= ImGui::Checkbox(LOCAL_LABEL("GraphicsSettings.VSync").c_str(), &m_DraftVSync);
        ImGui::EndDisabled();

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
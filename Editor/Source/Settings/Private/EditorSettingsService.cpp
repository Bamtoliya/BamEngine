#include "EditorSettingsService.h"

#include "ImGuiManager.h"
#include "TimeManager.h"
#include "LocalizationManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "BamEditorSettings.gen.h"
#include "RHI.h"
#include "BaseViewportPanel.h"

#include <SDL3/SDL_filesystem.h>

namespace
{
    // 실패하면 기존 기본값을 유지하고 해당 파일의 덮어쓰기를 금지한다.
    template<typename T, typename Validator>
    bool ReadEditorSettingsFile(T& target, const std::filesystem::path& path,
        const reflection::Registry& registry, Validator validate, std::string_view name)
    {
        T candidate = target;
        const auto result = Engine::ConfigManager::Load(candidate, path, registry);

        if (result.Status == Engine::EConfigLoadStatus::Failed)
        {
            BAM_LOG(Warn, "Editor", "Config",
                "{} load failed: code={}, field={}, filesystem={}",
                name, static_cast<int>(result.Error.Code), result.Error.Field, result.FileError.message());
            return false;
        }

        if (!validate(candidate))
        {
            BAM_LOG(Warn, "Editor", "Config", "{} rejected: unsupported version or invalid value", name);
            return false;
        }

        target = std::move(candidate);
        return true;
    }

    template<typename T>
    Editor::EEditorSettingsSaveStatus WriteEditorSettingsFile(const T& value,
        const std::filesystem::path& path, const reflection::Registry& registry,
        bool canSave, std::string_view name)
    {
        using Status = Editor::EEditorSettingsSaveStatus;

        if (!canSave)
        {
            BAM_LOG(Warn, "Editor", "Config",
                "{} retained for this session; file persistence is unavailable", name);
            return Status::SessionOnly;
        }

        const auto result = Engine::ConfigManager::Save(value, path, registry);

        if (!result)
        {
            BAM_LOG(Warn, "Editor", "Config",
                "{} save failed: code={}, field={}, filesystem={}",
                name, static_cast<int>(result.Error.Code), result.Error.Field, result.FileError.message());
            return Status::Failed;
        }

        return Status::Saved;
    }
}

IMPLEMENT_SINGLETON(EditorSettingsService)

#pragma region Constructor & Destructor
EResult EditorSettingsService::Initialize(void*)
{
    return EResult::Success;
}

void EditorSettingsService::Free()
{
    UnbindRuntime();
}
#pragma endregion

bool EditorSettingsService::LoadForStartup(std::uint32_t defaultWidth, std::uint32_t defaultHeight)
{
    if (IsWindowChangePending() || IsVSyncChangePending() || IsViewportResolutionChangePending())
        return false;

    m_WindowCaptureBlocked = false;

    m_CanSaveEditorPreferences = false;
    m_CanSaveEditorWorkspace = false;

    m_EditorPreferences = {};
    m_EditorWorkspace = {};
    m_ProjectSettings = {};
    m_SettingsRegistry = {};

    m_EditorPreferencesPath.clear();
    m_EditorWorkspacePath.clear();
    m_ProjectRoot.clear();

    if (defaultWidth < 320 || defaultWidth > 16384 ||
        defaultHeight < 240 || defaultHeight > 16384)
    {
        BAM_LOG(Error, "Editor", "Config", "Invalid initial editor window size");
        return false;
    }

    m_EditorWorkspace.WindowWidth = static_cast<std::int32_t>(defaultWidth);
    m_EditorWorkspace.WindowHeight = static_cast<std::int32_t>(defaultHeight);

    std::error_code error;
    m_ProjectRoot = std::filesystem::current_path(error);

    if (error)
    {
        BAM_LOG(Error, "Editor", "Config", "Cannot resolve project root: {}", error.message());
        return false;
    }

    reflection::RegistrationResult registration;
    if (!reflection_generated::Register_BamEditorSettings(m_SettingsRegistry, registration))
    {
        BAM_LOG(Error, "Editor", "Config",
            "Editor settings registration failed: error={}, owner={}, member={}",
            reflection::ToString(registration.Error), registration.Owner, registration.Member);
        return false;
    }

    std::unique_ptr<char, decltype(&SDL_free)> directory(
        SDL_GetPrefPath("BamEngine", "Editor"), &SDL_free);

    if (directory)
    {
        m_EditorPreferencesPath =
            std::filesystem::u8path(directory.get()) / "EditorPreferences.json";

        m_CanSaveEditorPreferences = ReadEditorSettingsFile(
            m_EditorPreferences, m_EditorPreferencesPath, m_SettingsRegistry,
            [](const EditorPreferences& value) { return value.HasValidValues(); }, "EditorPreferences");
    }
    else
    {
        BAM_LOG(Warn, "Editor", "Config",
            "Editor preferences directory unavailable: {}", SDL_GetError());
    }

    m_EditorWorkspacePath = m_ProjectRoot / ".bam" / "User" / "Workspace.json";

    m_CanSaveEditorWorkspace = ReadEditorSettingsFile(
        m_EditorWorkspace, m_EditorWorkspacePath, m_SettingsRegistry,
        [](const EditorWorkspace& value)
        {
            return value.SchemaVersion == EditorWorkspace::CurrentSchemaVersion &&
                value.HasValidWindowSize();
        }, "EditorWorkspace");

    return true;
}

bool EditorSettingsService::LoadProjectSettings(const reflection::Registry& registry)
{
    if (m_ProjectRoot.empty())
        return false;

    return ReadEditorSettingsFile(
        m_ProjectSettings, m_ProjectRoot / "Config" / "Project.json", registry,
        [](const Engine::ProjectSettings& value)
        {
            if (value.SchemaVersion != Engine::ProjectSettings::CurrentSchemaVersion)
                return false;

            const std::filesystem::path manifest(value.StartupManifestPath);
            return !manifest.empty() && !manifest.has_root_name() && !manifest.has_root_directory();
        }, "ProjectSettings");
}

void EditorSettingsService::CaptureWindowState(const EditorApplicationState& state)
{
    if (m_WindowPhase != EWindowChangePhase::Idle || m_WindowCaptureBlocked)
        return;

    CommitWindowState(state);
}

void EditorSettingsService::CommitWindowState(const EditorApplicationState& state)
{
    if (!state.HasWindow || state.WindowMinimized)
        return;

    m_EditorWorkspace.WindowFullscreen = state.WindowFullscreen;

    if (state.WindowFullscreen)
        return;

    m_EditorWorkspace.WindowMaximized = state.WindowMaximized;

    if (state.WindowMaximized || !state.HasWindowSize ||
        !EditorWorkspace::HasValidWindowSize(state.WindowWidth, state.WindowHeight))
    {
        return;
    }

    m_EditorWorkspace.WindowWidth = state.WindowWidth;
    m_EditorWorkspace.WindowHeight = state.WindowHeight;
}

void EditorSettingsService::CaptureWorkspace(ImGuiManager& gui)
{
    gui.CaptureWorkspace(m_EditorWorkspace);
}

std::vector<EditorViewportResolutionSnapshot> EditorSettingsService::GetViewportResolutionStates() const
{
    std::vector<EditorViewportResolutionSnapshot> states;

    if (!m_ImGuiManager)
        return states;

    states.reserve(2);

    // 현재 저장·복원 범위인 기본 렌더 뷰포트만 제공한다.
    for (const auto* name : { L"Scene View", L"Game View" })
    {
        auto* viewport = dynamic_cast<BaseViewportPanel*>(m_ImGuiManager->GetImGuiPanel(name));

        if (!viewport)
            continue;

        states.push_back({
            viewport->GetName(), viewport->GetDisplayName(), viewport->CaptureResolutionState()
            });
    }

    return states;
}

BaseViewportPanel* EditorSettingsService::FindViewportResolutionTarget(const std::wstring& name) const
{
    if (!m_ImGuiManager || (name != L"Scene View" && name != L"Game View"))
        return nullptr;

    return dynamic_cast<BaseViewportPanel*>(m_ImGuiManager->GetImGuiPanel(name));
}

EResult EditorSettingsService::RequestViewportResolutionChange(
    const std::wstring& name, const ViewportWorkspace& state)
{
    if (!m_ImGuiManager || IsWindowChangePending() || IsVSyncChangePending() ||
        IsViewportResolutionChangePending())
    {
        return EResult::AccessDenied;
    }

    if (!FindViewportResolutionTarget(name))
        return EResult::InvalidArgument;

    m_PendingViewportRequest = ViewportResolutionRequest{ name, state, {}, false };
    return EResult::Success;
}

void EditorSettingsService::ProcessPendingViewportResolutionChange()
{
    if (!m_PendingViewportRequest || m_PendingViewportRequest->Submitted)
        return;

    auto& request = *m_PendingViewportRequest;

    if (auto* viewport = FindViewportResolutionTarget(request.Name))
    {
        request.PreviousState = viewport->CaptureResolutionState();

        if (viewport->TrySetResolutionState(request.State))
        {
            request.Submitted = true;
            return;
        }
    }

    m_ViewportRequestResult = EditorViewportResolutionResult{ request.Name, {}, true };
    m_PendingViewportRequest.reset();
}

void EditorSettingsService::CompleteViewportResolutionChange()
{
    if (!m_PendingViewportRequest || !m_PendingViewportRequest->Submitted)
        return;

    const auto& request = *m_PendingViewportRequest;
    EditorViewportResolutionResult result;
    result.Name = request.Name;

    if (auto* viewport = FindViewportResolutionTarget(request.Name))
    {
        if (viewport->GetLastResolutionUpdateResult() == EResult::Success)
        {
            viewport->CaptureState(m_EditorWorkspace);
            result.Change = { true, SaveWorkspace() };
        }
        else
        {
            const bool restored = viewport->TrySetResolutionState(request.PreviousState);
            result.RenderTargetsValid = restored && viewport->HasValidRenderTargets();

            if (!restored)
            {
                BAM_LOG(Error, "Editor", "Config",
                    "Viewport resolution settings could not be restored: {}", WStrToStr(request.Name));
            }
        }
    }

    m_ViewportRequestResult = std::move(result);
    m_PendingViewportRequest.reset();
}

std::optional<EditorViewportResolutionResult> EditorSettingsService::TakeViewportResolutionResult()
{
    auto result = std::move(m_ViewportRequestResult);
    m_ViewportRequestResult.reset();
    return result;
}

EEditorSettingsSaveStatus EditorSettingsService::SaveWorkspace() const
{
    if (m_EditorWorkspace.SchemaVersion != EditorWorkspace::CurrentSchemaVersion ||
        !m_EditorWorkspace.HasValidWindowSize())
    {
        BAM_LOG(Error, "Editor", "Config", "Invalid editor workspace was not saved");
        return EEditorSettingsSaveStatus::Failed;
    }

    return WriteEditorSettingsFile(
        m_EditorWorkspace, m_EditorWorkspacePath, m_SettingsRegistry,
        m_CanSaveEditorWorkspace, "EditorWorkspace");
}

void EditorSettingsService::RememberEditedScene(const std::filesystem::path& path)
{
    if (path.empty())
    {
        m_EditorWorkspace.LastScenePath.clear();
        return;
    }

    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);

    if (error)
    {
        BAM_LOG(Warn, "Editor", "Workspace",
            "Cannot resolve edited scene path: {}", error.message());
        return;
    }

    absolute = absolute.lexically_normal();

    const auto relative = absolute.lexically_relative(m_ProjectRoot);
    const bool insideProject = !relative.empty() &&
        *relative.begin() != std::filesystem::path("..");

    const auto stored = insideProject ? relative : absolute;
    const auto utf8 = stored.generic_u8string();
    m_EditorWorkspace.LastScenePath.assign(utf8.begin(), utf8.end());
}

void EditorSettingsService::BindRuntime(ImGuiManager& gui, TimeManager& time, LocalizationManager& localization, RHI& rhi)
{
    m_ImGuiManager = &gui;
    m_TimeManager = &time;
    m_LocalizationManager = &localization;
    m_RHI = &rhi;
}

void EditorSettingsService::UnbindRuntime()
{
    m_WindowCaptureBlocked = m_WindowCaptureBlocked || m_WindowPhase != EWindowChangePhase::Idle;

    m_ImGuiManager = nullptr;
    m_TimeManager = nullptr;
    m_LocalizationManager = nullptr;
    m_RHI = nullptr;

    m_PendingVSyncRequest.reset();
    m_VSyncRequestResult.reset();
    m_WindowPhase = EWindowChangePhase::Idle;
    m_PendingWindowRequest.reset();
    m_SubmittedWindowRequest.reset();
    m_WindowRequestResult.reset();
    m_WindowRequestDeadline = {};
    m_WindowConfirmationDeadline = {};
    m_PendingViewportRequest.reset();
    m_ViewportRequestResult.reset();
}

bool EditorSettingsService::ApplyPreferences(const EditorPreferences& preferences)
{
    if (!m_ImGuiManager || !m_TimeManager || !m_LocalizationManager)
    {
        BAM_LOG(Warn, "Editor", "Config", "Editor settings runtime is not bound");
        return false;
    }

    if (!preferences.HasValidValues())
    {
        BAM_LOG(Warn, "Editor", "Config", "Invalid editor preferences");
        return false;
    }

    const auto language = preferences.Language == "English"
        ? ELocalizationLanguage::English : ELocalizationLanguage::Korean;

    if (m_LocalizationManager->SetCurrentLanguage(language) != Engine::EResult::Success)
        return false;

    m_ImGuiManager->SetUIScale(preferences.UIScale);
    m_TimeManager->SetTargetFPS(static_cast<uint32>(preferences.TargetFPS));

    m_EditorPreferences = preferences;
    return true;
}

bool EditorSettingsService::RestoreEditorState()
{
    if (!ApplyPreferences(m_EditorPreferences))
        return false;

    m_ImGuiManager->RestoreWorkspace(m_EditorWorkspace);
    return true;
}

void EditorSettingsService::RestoreStartupVSync(Engine::RHI& rhi) const
{
    const bool requested = m_EditorPreferences.VSync;
    auto result = EResult::Success;

    if (rhi.IsVSyncEnabled() != requested)
        result = rhi.SupportsVSync(requested) ? rhi.SetVSync(requested) : EResult::NotImplemented;

    if (result != EResult::Success)
    {
        BAM_LOG(Warn, "Editor", "Config",
            "Startup VSync was not applied: requested={}, actual={}, result={}. Stored preference was preserved.",
            requested, rhi.IsVSyncEnabled(), ResultToString(result));
        return;
    }

    BAM_LOG(Info, "Editor", "Config", "Startup VSync: requested={}, actual={}",
        requested, rhi.IsVSyncEnabled());
}

std::optional<bool> EditorSettingsService::GetEditorVSync() const
{
    if (!m_RHI)
        return std::nullopt;

    return m_RHI->IsVSyncEnabled();
}

bool EditorSettingsService::SupportsEditorVSync(bool enabled) const
{
    return m_RHI && m_RHI->SupportsVSync(enabled);
}

EResult EditorSettingsService::RequestVSyncChange(bool enabled)
{
    if (!m_RHI || IsWindowChangePending() || IsVSyncChangePending() || IsViewportResolutionChangePending())
        return EResult::AccessDenied;

    if (m_RHI->IsVSyncEnabled() != enabled && !m_RHI->SupportsVSync(enabled))
        return EResult::NotImplemented;

    m_PendingVSyncRequest = enabled;
    return EResult::Success;
}

void EditorSettingsService::ProcessPendingVSyncChange()
{
    if (!m_PendingVSyncRequest)
        return;

    const bool requested = *m_PendingVSyncRequest;
    m_PendingVSyncRequest.reset();

    auto result = EResult::AccessDenied;

    if (m_RHI)
    {
        result = EResult::Success;

        if (m_RHI->IsVSyncEnabled() != requested)
            result = m_RHI->SupportsVSync(requested) ? m_RHI->SetVSync(requested) : EResult::NotImplemented;

        if (result == EResult::Success && m_RHI->IsVSyncEnabled() != requested)
            result = EResult::Fail;
    }

    if (result != EResult::Success)
    {
        BAM_LOG(Warn, "Editor", "Config", "VSync change failed: requested={}, result={}",
            requested, ResultToString(result));

        m_VSyncRequestResult = EditorSettingsChangeResult{};
        return;
    }

    m_EditorPreferences.VSync = m_RHI->IsVSyncEnabled();
    m_VSyncRequestResult = EditorSettingsChangeResult{ true, SavePreferences() };

    BAM_LOG(Info, "Editor", "Config", "Editor VSync applied: enabled={}", m_EditorPreferences.VSync);
}

std::optional<EditorSettingsChangeResult> EditorSettingsService::TakeVSyncRequestResult()
{
    const auto result = m_VSyncRequestResult;
    m_VSyncRequestResult.reset();
    return result;
}

EditorSettingsChangeResult EditorSettingsService::ApplyAndSavePreferences(const EditorPreferences& preferences)
{
    if (!ApplyPreferences(preferences))
        return {};

    return { true, SavePreferences() };
}

EditorSettingsChangeResult EditorSettingsService::SetEditorUIScale(float scale)
{
    EditorPreferences candidate = m_EditorPreferences;
    candidate.UIScale = scale;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetEditorTargetFPS(std::int32_t fps)
{
    EditorPreferences candidate = m_EditorPreferences;
    candidate.TargetFPS = fps;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetEditorLanguage(ELocalizationLanguage language)
{
    if (language != ELocalizationLanguage::English && language != ELocalizationLanguage::Korean)
    {
        BAM_LOG(Warn, "Editor", "Config", "Invalid editor language");
        return {};
    }

    EditorPreferences candidate = m_EditorPreferences;
    candidate.Language = language == ELocalizationLanguage::English ? "English" : "Korean";
    return ApplyAndSavePreferences(candidate);
}

EEditorSettingsSaveStatus EditorSettingsService::SavePreferences() const
{
    return WriteEditorSettingsFile(
        m_EditorPreferences, m_EditorPreferencesPath, m_SettingsRegistry,
        m_CanSaveEditorPreferences, "EditorPreferences");
}

EResult EditorSettingsService::RequestWindowChange(const EditorWindowRequest& request)
{
    if (!request.HasValidValues())
        return EResult::InvalidArgument;

    if (IsWindowChangePending() || IsVSyncChangePending() || IsViewportResolutionChangePending())
        return EResult::AccessDenied;

    m_PendingWindowRequest = request;
    m_SubmittedWindowRequest.reset();
    m_WindowRequestResult.reset();
    m_WindowPhase = EWindowChangePhase::Queued;
    return EResult::Success;
}

void EditorSettingsService::ProcessPendingWindowChange(const EditorApplicationState& state, const std::function<EResult(const EditorWindowRequest&)>& submit)
{
    const bool restoring = m_WindowPhase == EWindowChangePhase::RestoreQueued;

    if ((m_WindowPhase != EWindowChangePhase::Queued && !restoring) || !m_PendingWindowRequest)
        return;

    if (!state.HasWindow || (!restoring && (!state.HasWindowSize || state.WindowMinimized)))
    {
        FinishWindowChange(restoring ? EEditorWindowRequestStatus::RestoreFailed
            : EEditorWindowRequestStatus::SubmissionFailed, state);
        return;
    }

    if (!restoring)
    {
        if (!m_WindowCaptureBlocked)
            CommitWindowState(state);

        m_WindowRestoreRequest = {
            m_EditorWorkspace.WindowWidth, m_EditorWorkspace.WindowHeight,
            m_EditorWorkspace.WindowFullscreen, m_EditorWorkspace.WindowMaximized
        };
    }

    auto request = *m_PendingWindowRequest;
    m_PendingWindowRequest.reset();

    if (!restoring && request.Fullscreen)
    {
        request.Width = m_WindowRestoreRequest.Width;
        request.Height = m_WindowRestoreRequest.Height;
        request.Maximized = m_WindowRestoreRequest.Maximized;
    }

    const auto result = submit ? submit(request) : EResult::InvalidArgument;

    if (result != EResult::Success)
    {
        if (restoring)
            FinishWindowChange(EEditorWindowRequestStatus::RestoreFailed, state);
        else
            QueueWindowRestore(EEditorWindowRequestStatus::SubmissionFailed);

        return;
    }

    m_SubmittedWindowRequest = request;
    m_WindowPhase = restoring ? EWindowChangePhase::Restoring : EWindowChangePhase::Applying;
    m_WindowRequestDeadline = std::chrono::steady_clock::now() + WindowApplyTimeout;
}

bool EditorSettingsService::MatchesWindowRequest(
    const EditorWindowRequest& request, const EditorApplicationState& state)
{
    if (!state.HasWindow || !state.HasWindowSize || state.WindowMinimized ||
        state.WindowFullscreen != request.Fullscreen)
    {
        return false;
    }

    if (request.Fullscreen)
        return true;

    return state.WindowMaximized == request.Maximized &&
        (request.Maximized || (state.WindowWidth == request.Width && state.WindowHeight == request.Height));
}

void EditorSettingsService::ObserveWindowChange(const EditorApplicationState& state, bool outputReady)
{
    if (m_WindowPhase == EWindowChangePhase::Idle || m_WindowPhase == EWindowChangePhase::Queued ||
        m_WindowPhase == EWindowChangePhase::RestoreQueued)
    {
        return;
    }

    if (!state.HasWindow || !m_SubmittedWindowRequest)
    {
        FinishWindowChange(EEditorWindowRequestStatus::RestoreFailed, state);
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const bool ready = outputReady && MatchesWindowRequest(*m_SubmittedWindowRequest, state);

    if (m_WindowPhase == EWindowChangePhase::Applying || m_WindowPhase == EWindowChangePhase::Restoring)
    {
        if (now >= m_WindowRequestDeadline)
        {
            if (m_WindowPhase == EWindowChangePhase::Restoring)
                FinishWindowChange(EEditorWindowRequestStatus::RestoreFailed, state);
            else
                QueueWindowRestore(EEditorWindowRequestStatus::TimedOut);

            return;
        }

        if (!ready)
            return;

        if (m_WindowPhase == EWindowChangePhase::Restoring)
        {
            m_WindowCaptureBlocked = false;
            FinishWindowChange(m_WindowRestoreReason, state);
            return;
        }

        m_WindowPhase = EWindowChangePhase::AwaitingConfirmation;
        m_WindowConfirmationDeadline = now + WindowConfirmationTimeout;
        return;
    }

    if (!ready)
    {
        QueueWindowRestore(EEditorWindowRequestStatus::Cancelled);
        return;
    }

    if (m_WindowPhase == EWindowChangePhase::AwaitingConfirmation)
    {
        if (now >= m_WindowConfirmationDeadline)
            QueueWindowRestore(EEditorWindowRequestStatus::ConfirmationTimedOut);

        return;
    }

    if (m_WindowPhase == EWindowChangePhase::CommitQueued)
    {
        m_EditorWorkspace.WindowWidth = m_SubmittedWindowRequest->Width;
        m_EditorWorkspace.WindowHeight = m_SubmittedWindowRequest->Height;
        m_EditorWorkspace.WindowMaximized = m_SubmittedWindowRequest->Maximized;
        CommitWindowState(state);

        if (m_ImGuiManager)
            CaptureWorkspace(*m_ImGuiManager);

        const auto saveStatus = SaveWorkspace();

        m_WindowCaptureBlocked = false;
        FinishWindowChange(EEditorWindowRequestStatus::Confirmed, state);
        m_WindowRequestResult->SaveStatus = saveStatus;
    }
}

EResult EditorSettingsService::ConfirmWindowChange()
{
    if (!IsAwaitingWindowConfirmation())
        return EResult::AccessDenied;

    if (std::chrono::steady_clock::now() >= m_WindowConfirmationDeadline)
    {
        QueueWindowRestore(EEditorWindowRequestStatus::ConfirmationTimedOut);
        return EResult::AccessDenied;
    }

    m_WindowPhase = EWindowChangePhase::CommitQueued;
    return EResult::Success;
}

EResult EditorSettingsService::CancelWindowChange()
{
    if (!IsAwaitingWindowConfirmation())
        return EResult::AccessDenied;

    const auto reason = std::chrono::steady_clock::now() >= m_WindowConfirmationDeadline
        ? EEditorWindowRequestStatus::ConfirmationTimedOut : EEditorWindowRequestStatus::Cancelled;

    QueueWindowRestore(reason);
    return EResult::Success;
}

std::int32_t EditorSettingsService::GetWindowConfirmationSeconds() const
{
    if (!IsAwaitingWindowConfirmation())
        return 0;

    const auto remaining = std::chrono::ceil<std::chrono::seconds>(
        m_WindowConfirmationDeadline - std::chrono::steady_clock::now()).count();

    return static_cast<std::int32_t>(std::max<std::int64_t>(0, remaining));
}

void EditorSettingsService::QueueWindowRestore(EEditorWindowRequestStatus reason)
{
    m_WindowRestoreReason = reason;
    m_PendingWindowRequest = m_WindowRestoreRequest;
    m_SubmittedWindowRequest.reset();
    m_WindowPhase = EWindowChangePhase::RestoreQueued;
    m_WindowCaptureBlocked = true;
}

void EditorSettingsService::FinishWindowChange(
    EEditorWindowRequestStatus status, const EditorApplicationState& state)
{
    if (status == EEditorWindowRequestStatus::RestoreFailed)
        m_WindowCaptureBlocked = true;

    m_WindowPhase = EWindowChangePhase::Idle;
    m_PendingWindowRequest.reset();
    m_SubmittedWindowRequest.reset();
    m_WindowRequestResult = EditorWindowRequestResult{ status, state, std::nullopt };
}

std::optional<EditorWindowRequestResult> EditorSettingsService::TakeWindowRequestResult()
{
    const auto result = m_WindowRequestResult;
    m_WindowRequestResult.reset();
    return result;
}
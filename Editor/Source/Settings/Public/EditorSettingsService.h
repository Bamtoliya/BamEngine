#pragma once

#include "Editor_Includes.h"
#include "EditorSettings.h"
#include "Config.h"
#include <reflection/Registry.h>
#include "LocalizationManager.h"

BEGIN(Engine)
class RHI;
class TimeManager;
class LocalizationManager;
struct InputActionSettings;
END

BEGIN(Editor)
class ImGuiManager;
class BaseViewportPanel;
enum class EEditorSettingsSaveStatus
{
    Saved,
    SessionOnly,
    Failed
};

struct EditorSettingsChangeResult
{
    bool Applied = false;
    std::optional<EEditorSettingsSaveStatus> SaveStatus;
};

enum class EEditorWindowRequestStatus
{
    Confirmed,
    Cancelled,
    ConfirmationTimedOut,
    SubmissionFailed,
    TimedOut,
    RestoreFailed
};

struct EditorWindowRequestResult
{
    EEditorWindowRequestStatus Status = EEditorWindowRequestStatus::SubmissionFailed;
    EditorApplicationState ActualState;
    std::optional<EEditorSettingsSaveStatus> SaveStatus;
};

struct EditorViewportResolutionSnapshot
{
    std::wstring Name;
    std::string DisplayName;
    ViewportWorkspace State;
};

struct EditorViewportResolutionResult
{
    std::wstring Name;
    EditorSettingsChangeResult Change;
    bool RenderTargetsValid = true;
};

class EditorSettingsService final : public Base
{
    DECLARE_SINGLETON(EditorSettingsService)
#pragma region Constructor&Destructor
private:
    EditorSettingsService() = default;
    ~EditorSettingsService() override = default;
    EResult Initialize(void* arg = nullptr);
public:
    void Free() override;
#pragma endregion
public:
    bool LoadForStartup(std::uint32_t defaultWidth, std::uint32_t defaultHeight);
    bool LoadProjectSettings(const reflection::Registry& registry);
    bool LoadInputActionSettings(Engine::InputActionSettings& out) const;
    EEditorSettingsSaveStatus SaveProjectSettings(const Engine::ProjectSettings& settings);
    EEditorSettingsSaveStatus SaveInputActionSettings(const Engine::InputActionSettings& settings) const;

    void CaptureWindowState(const EditorApplicationState& state);
    void CaptureWorkspace(ImGuiManager& gui);

    EEditorSettingsSaveStatus SaveWorkspace() const;

    void RememberEditedScene(const std::filesystem::path& path);

public:
    void BindRuntime(ImGuiManager& gui, Engine::TimeManager& time, Engine::LocalizationManager& localization, Engine::RHI& rhi);
    void UnbindRuntime();

    bool RestoreEditorState();
    void RestoreStartupVSync(Engine::RHI& rhi) const;

    [[nodiscard]] std::optional<bool> GetEditorVSync() const;
    [[nodiscard]] bool SupportsEditorVSync(bool enabled) const;
    [[nodiscard]] EResult RequestVSyncChange(bool enabled);

    void ProcessPendingVSyncChange();
    [[nodiscard]] std::optional<EditorSettingsChangeResult> TakeVSyncRequestResult();

    [[nodiscard]] bool IsVSyncChangePending() const
    {
        return m_PendingVSyncRequest.has_value() || m_VSyncRequestResult.has_value();
    }

    [[nodiscard]] EditorSettingsChangeResult SetEditorUIScale(float scale);
    [[nodiscard]] EditorSettingsChangeResult SetEditorTargetFPS(std::int32_t fps);
    [[nodiscard]] EditorSettingsChangeResult SetEditorLanguage(ELocalizationLanguage language);
    [[nodiscard]] EditorSettingsChangeResult SetGizmoSnapping(float translation, float rotation, float scale);
    [[nodiscard]] EditorSettingsChangeResult SetGizmoSnapTranslation(float snap);
    [[nodiscard]] EditorSettingsChangeResult SetGizmoSnapRotation(float snap);
    [[nodiscard]] EditorSettingsChangeResult SetGizmoSnapScale(float snap);
    [[nodiscard]] EditorSettingsChangeResult SetEditorPreferences(const EditorPreferences& preferences);
    [[nodiscard]] EditorSettingsChangeResult SetPreviewLightIntensity(const std::array<float, 3>& intensities);

    [[nodiscard]] std::vector<EditorViewportResolutionSnapshot> GetViewportResolutionStates() const;

    [[nodiscard]] EResult RequestViewportResolutionChange(const std::wstring& name, const ViewportWorkspace& state);
    void ProcessPendingViewportResolutionChange();
    void CompleteViewportResolutionChange();
    [[nodiscard]] std::optional<EditorViewportResolutionResult> TakeViewportResolutionResult();

    [[nodiscard]] bool IsViewportResolutionChangePending() const
    {
        return m_PendingViewportRequest.has_value() || m_ViewportRequestResult.has_value();
    }
private:
    BaseViewportPanel* FindViewportResolutionTarget(const std::wstring& name) const;
private:
    bool ApplyPreferences(const EditorPreferences& preferences);
    EditorSettingsChangeResult ApplyAndSavePreferences(const EditorPreferences& preferences);
    EEditorSettingsSaveStatus SavePreferences() const;

public:
    [[nodiscard]] const EditorPreferences& GetPreferences() const;
    [[nodiscard]] const EditorWorkspace& GetWorkspace() const;
    [[nodiscard]] const Engine::ProjectSettings& GetProjectSettings() const;
    [[nodiscard]] const std::filesystem::path& GetProjectRoot() const;
    [[nodiscard]] const std::filesystem::path& GetPreferencesPath() const;
    [[nodiscard]] const std::filesystem::path& GetWorkspacePath() const;

    [[nodiscard]] bool IsPreferencesSaveBlocked() const { return !m_CanSaveEditorPreferences; }
    [[nodiscard]] bool IsWorkspaceSaveBlocked() const { return !m_CanSaveEditorWorkspace; }

private:
    enum class EWindowChangePhase
    {
        Idle,
        Queued,
        Applying,
        AwaitingConfirmation,
        CommitQueued,
        RestoreQueued,
        Restoring
    };

public:
    [[nodiscard]] EResult RequestWindowChange(const EditorWindowRequest& request);
    void ProcessPendingWindowChange(const EditorApplicationState& state, const std::function<EResult(const EditorWindowRequest&)>& submit);
    void ObserveWindowChange(const EditorApplicationState& state, bool outputReady);

    [[nodiscard]] EResult ConfirmWindowChange();
    [[nodiscard]] EResult CancelWindowChange();
    [[nodiscard]] std::int32_t GetWindowConfirmationSeconds() const;
    [[nodiscard]] std::optional<EditorWindowRequestResult> TakeWindowRequestResult();

    [[nodiscard]] bool IsWindowChangePending() const
    {
        return m_WindowPhase != EWindowChangePhase::Idle || m_WindowRequestResult.has_value();
    }

    [[nodiscard]] bool IsAwaitingWindowConfirmation() const
    {
        return m_WindowPhase == EWindowChangePhase::AwaitingConfirmation;
    }

    [[nodiscard]] bool IsWindowCaptureBlocked() const { return m_WindowCaptureBlocked; }

private:
    static bool MatchesWindowRequest(const EditorWindowRequest& request, const EditorApplicationState& state);
    void CommitWindowState(const EditorApplicationState& state);
    void QueueWindowRestore(EEditorWindowRequestStatus reason);
    void FinishWindowChange(EEditorWindowRequestStatus status, const EditorApplicationState& state);

private:
    reflection::Registry m_SettingsRegistry;

    EditorPreferences m_EditorPreferences;
    std::filesystem::path m_EditorPreferencesPath;
    bool m_CanSaveEditorPreferences = false;

    EditorWorkspace m_EditorWorkspace;
    std::filesystem::path m_EditorWorkspacePath;
    bool m_CanSaveEditorWorkspace = false;

    Engine::ProjectSettings m_ProjectSettings;
    std::filesystem::path m_ProjectRoot;

    ImGuiManager* m_ImGuiManager = nullptr;
    Engine::TimeManager* m_TimeManager = nullptr;
    Engine::LocalizationManager* m_LocalizationManager = nullptr;
    Engine::RHI* m_RHI = nullptr;

    std::optional<bool> m_PendingVSyncRequest;
    std::optional<EditorSettingsChangeResult> m_VSyncRequestResult;

private:
    static constexpr std::chrono::seconds WindowApplyTimeout{ 5 };
    static constexpr std::chrono::seconds WindowConfirmationTimeout{ 15 };

    EWindowChangePhase m_WindowPhase = EWindowChangePhase::Idle;
    EEditorWindowRequestStatus m_WindowRestoreReason = EEditorWindowRequestStatus::Cancelled;

    EditorWindowRequest m_WindowRestoreRequest;
    std::optional<EditorWindowRequest> m_PendingWindowRequest;
    std::optional<EditorWindowRequest> m_SubmittedWindowRequest;
    std::optional<EditorWindowRequestResult> m_WindowRequestResult;

    std::chrono::steady_clock::time_point m_WindowRequestDeadline{};
    std::chrono::steady_clock::time_point m_WindowConfirmationDeadline{};
    bool m_WindowCaptureBlocked = false;

private:
    struct ViewportResolutionRequest
    {
        std::wstring Name;
        ViewportWorkspace State;
        ViewportWorkspace PreviousState;
        bool Submitted = false;
    };

    std::optional<ViewportResolutionRequest> m_PendingViewportRequest;
    std::optional<EditorViewportResolutionResult> m_ViewportRequestResult;
};
END
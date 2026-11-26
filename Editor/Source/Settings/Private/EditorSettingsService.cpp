#include "EditorSettingsService.h"
#include "AmbientOcclusion.h"
#include "RaytracedShadowSettings.h"

#include "ImGuiManager.h"
#include "TimeManager.h"
#include "LocalizationManager.h"
#include "ConfigManager.h"
#include "Logger.h"
#include "RuntimeSettingsManager.h"
#include "BamEditorSettings.gen.h"
#include "RHI.h"
#include "BaseViewportPanel.h"

#include <SDL3/SDL_filesystem.h>
#ifdef new
#undef new
#define BAM_RESTORE_DBG_NEW_SETTINGS
#endif
#include <glaze/glaze.hpp>
#ifdef BAM_RESTORE_DBG_NEW_SETTINGS
#undef BAM_RESTORE_DBG_NEW_SETTINGS
#define new DBG_NEW
#endif
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include "PlatformPath.h"
#include "InputActionSettings.h"
#include "InputManager.h"
#include "ReflectionRegistry.h"

namespace Editor
{
    struct RecentProjectsDocument
    {
        std::uint32_t SchemaVersion = 1;
        std::vector<RecentProjectEntry> Projects;
    };
}

namespace
{
    std::filesystem::path ResolveRecentProjectsFilePath()
    {
        std::unique_ptr<char, decltype(&SDL_free)> directory(
            SDL_GetPrefPath("BamEngine", "Editor"), &SDL_free);
        if (directory && directory.get()[0] != '\0')
        {
            std::filesystem::path prefDir = std::filesystem::u8path(directory.get()).lexically_normal();
            if (!prefDir.has_filename() && prefDir.has_parent_path())
                prefDir = prefDir.parent_path();
            const std::filesystem::path bamEngineDir = prefDir.has_parent_path() ? prefDir.parent_path() : prefDir;
            return bamEngineDir / "RecentProjects.json";
        }

#ifdef _WIN32
        char* appData = nullptr;
        size_t len = 0;
        if (_dupenv_s(&appData, &len, "APPDATA") == 0 && appData != nullptr)
        {
            const std::filesystem::path result = std::filesystem::u8path(appData) / "BamEngine" / "RecentProjects.json";
            std::free(appData);
            return result;
        }
#else
        if (const char* home = std::getenv("HOME"))
            return std::filesystem::u8path(home) / ".config" / "BamEngine" / "RecentProjects.json";
#endif
        return {};
    }

    std::string FormatUtcIso8601Now()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t rawTime = std::chrono::system_clock::to_time_t(now);
        std::tm utcTm{};
#ifdef _WIN32
        ::gmtime_s(&utcTm, &rawTime);
#else
        ::gmtime_r(&rawTime, &utcTm);
#endif
        char buffer[32]{};
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02dT%02d:%02d:%02dZ",
            utcTm.tm_year + 1900, utcTm.tm_mon + 1, utcTm.tm_mday,
            utcTm.tm_hour, utcTm.tm_min, utcTm.tm_sec);
        return std::string(buffer);
    }

    std::string PathToUtf8(const std::filesystem::path& path)
    {
        const auto u8 = Engine::PlatformPath::Normalize(path).generic_u8string();
        return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
    }

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
    m_ProjectRoot.clear();

    // 스플래시 전 StartupProjectHub 등에서 이미 프로젝트 루트가 지정된 경우 이를 최우선으로 사용한다.
    std::error_code ec;
    const auto preselectedRoot = Engine::PlatformPath::GetProjectRoot();
    if (!preselectedRoot.empty() && std::filesystem::exists(preselectedRoot, ec))
    {
        m_ProjectRoot = Engine::PlatformPath::Normalize(preselectedRoot);
    }

    // 미지정 시 실행 파일 위치부터 부모로 거슬러 올라가며 프로젝트 루트 마커 탐색
    if (m_ProjectRoot.empty())
    {
        std::filesystem::path cur = Engine::PlatformPath::GetExecutableDirectory();
        for (int i = 0; i < 7; ++i)
        {
            if (std::filesystem::exists(cur / "Config" / "Project.json", ec))
            {
                m_ProjectRoot = cur;
                break;
            }
            if (!cur.has_parent_path()) break;
            cur = cur.parent_path();
        }
    }
    if (m_ProjectRoot.empty())
    {
        m_ProjectRoot = std::filesystem::current_path(ec);
        BAM_LOG(Warn, "Editor", "Config", "Project marker not found; falling back to current_path: {}", m_ProjectRoot.string());
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

    m_RecentProjectsPath = ResolveRecentProjectsFilePath();
    LoadRecentProjects();

    return true;
}

bool EditorSettingsService::ReloadProjectSettings(const std::filesystem::path& projectRoot)
{
    if (projectRoot.empty())
        return false;

    m_ProjectRoot = Engine::PlatformPath::Normalize(projectRoot);

    if (m_SettingsRegistry.FindType<EditorWorkspace>() == nullptr)
    {
        reflection::RegistrationResult registration;
        if (!reflection_generated::Register_BamEditorSettings(m_SettingsRegistry, registration))
        {
            BAM_LOG(Error, "Editor", "Config",
                "Editor settings registration failed during project reload: error={}, owner={}, member={}",
                reflection::ToString(registration.Error), registration.Owner, registration.Member);
            return false;
        }
    }

    // Keep global EditorPreferences untouched; reset and reload only project-scoped settings.
    m_ProjectSettings = {};
    bool projectSettingsOk = true;
    if (Engine::ReflectionRegistry::IsCreated())
    {
        projectSettingsOk = LoadProjectSettings(Engine::ReflectionRegistry::Get().GetRegistry());
    }

    Engine::InputActionSettings inputSettings;
    if (LoadInputActionSettings(inputSettings) && Engine::InputManager::IsCreated())
    {
        Engine::InputManager::Get().SetInputActionSettings(inputSettings);
    }

    const std::int32_t prevWidth = m_EditorWorkspace.WindowWidth > 0 ? m_EditorWorkspace.WindowWidth : 1280;
    const std::int32_t prevHeight = m_EditorWorkspace.WindowHeight > 0 ? m_EditorWorkspace.WindowHeight : 720;
    const bool prevMaximized = m_EditorWorkspace.WindowMaximized;
    const bool prevFullscreen = m_EditorWorkspace.WindowFullscreen;

    m_EditorWorkspace = {};
    m_EditorWorkspace.WindowWidth = prevWidth;
    m_EditorWorkspace.WindowHeight = prevHeight;
    m_EditorWorkspace.WindowMaximized = prevMaximized;
    m_EditorWorkspace.WindowFullscreen = prevFullscreen;

    std::error_code ec;
    const std::filesystem::path configWorkspacePath = m_ProjectRoot / "Config" / "EditorWorkspace.json";
    const std::filesystem::path legacyWorkspacePath = m_ProjectRoot / ".bam" / "User" / "Workspace.json";
    m_EditorWorkspacePath = (!std::filesystem::exists(configWorkspacePath, ec) && std::filesystem::exists(legacyWorkspacePath, ec))
        ? legacyWorkspacePath
        : configWorkspacePath;

    m_CanSaveEditorWorkspace = ReadEditorSettingsFile(
        m_EditorWorkspace, m_EditorWorkspacePath, m_SettingsRegistry,
        [](const EditorWorkspace& value)
        {
            return value.SchemaVersion == EditorWorkspace::CurrentSchemaVersion &&
                value.HasValidWindowSize();
        }, "EditorWorkspace");

    return projectSettingsOk;
}

bool EditorSettingsService::LoadRecentProjects()
{
    if (m_RecentProjectsPath.empty())
        m_RecentProjectsPath = ResolveRecentProjectsFilePath();
    if (m_RecentProjectsPath.empty())
        return false;

    std::error_code ec;
    if (!std::filesystem::exists(m_RecentProjectsPath, ec))
    {
        m_RecentProjects.clear();
        return true;
    }

    std::ifstream file(m_RecentProjectsPath, std::ios::binary);
    if (!file)
        return false;

    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    RecentProjectsDocument doc;
    const auto parseError = glz::read<glz::opts{ .error_on_unknown_keys = false }>(doc, content);
    if (parseError)
    {
        BAM_LOG(Warn, "Editor", "Config", "RecentProjects.json parse failed: {}", glz::format_error(parseError, content));
        return false;
    }

    m_RecentProjects = std::move(doc.Projects);
    if (m_RecentProjects.size() > MaxRecentProjects)
        m_RecentProjects.resize(MaxRecentProjects);
    return true;
}

bool EditorSettingsService::SaveRecentProjects() const
{
    const std::filesystem::path targetPath = m_RecentProjectsPath.empty() ? ResolveRecentProjectsFilePath() : m_RecentProjectsPath;
    if (targetPath.empty())
        return false;

    std::error_code ec;
    if (targetPath.has_parent_path())
        std::filesystem::create_directories(targetPath.parent_path(), ec);

    RecentProjectsDocument doc;
    doc.SchemaVersion = 1;
    doc.Projects = m_RecentProjects;

    std::string buffer;
    const auto writeError = glz::write<glz::opts{ .prettify = true }>(doc, buffer);
    if (writeError)
        return false;

    std::ofstream file(targetPath, std::ios::binary | std::ios::trunc);
    if (!file)
        return false;
    file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return file.good();
}

void EditorSettingsService::RecordRecentProject(const std::string& projectName, const std::filesystem::path& projectFileOrRoot)
{
    if (projectFileOrRoot.empty())
        return;

    if (m_RecentProjectsPath.empty())
        m_RecentProjectsPath = ResolveRecentProjectsFilePath();

    const std::filesystem::path normalized = Engine::PlatformPath::Normalize(projectFileOrRoot);
    std::error_code ec;
    const bool isDir = std::filesystem::is_directory(normalized, ec);
    const std::filesystem::path projectRoot = isDir ? normalized : normalized.parent_path();

    RecentProjectEntry entry;
    entry.ProjectName = projectName.empty() ? projectRoot.filename().string() : projectName;
    entry.ProjectFilePath = PathToUtf8(normalized);
    entry.ProjectRoot = PathToUtf8(projectRoot);
    entry.LastOpenedUtc = FormatUtcIso8601Now();

    m_RecentProjects.erase(
        std::remove_if(m_RecentProjects.begin(), m_RecentProjects.end(),
            [&normalized, &projectRoot](const RecentProjectEntry& existing)
            {
                const std::filesystem::path existingFile = std::filesystem::u8path(existing.ProjectFilePath);
                const std::filesystem::path existingRoot = std::filesystem::u8path(existing.ProjectRoot);
                return Engine::PlatformPath::IsSamePath(existingFile, normalized) ||
                    (!existingRoot.empty() && Engine::PlatformPath::IsSamePath(existingRoot, projectRoot));
            }),
        m_RecentProjects.end());

    m_RecentProjects.insert(m_RecentProjects.begin(), std::move(entry));
    if (m_RecentProjects.size() > MaxRecentProjects)
        m_RecentProjects.resize(MaxRecentProjects);

    SaveRecentProjects();
}

void EditorSettingsService::RemoveRecentProject(const std::filesystem::path& projectFileOrRoot)
{
    if (projectFileOrRoot.empty())
        return;

    const std::filesystem::path normalized = Engine::PlatformPath::Normalize(projectFileOrRoot);
    const auto beforeSize = m_RecentProjects.size();
    m_RecentProjects.erase(
        std::remove_if(m_RecentProjects.begin(), m_RecentProjects.end(),
            [&normalized](const RecentProjectEntry& existing)
            {
                const std::filesystem::path existingFile = std::filesystem::u8path(existing.ProjectFilePath);
                const std::filesystem::path existingRoot = std::filesystem::u8path(existing.ProjectRoot);
                return Engine::PlatformPath::IsSamePath(existingFile, normalized) ||
                    Engine::PlatformPath::IsSamePath(existingRoot, normalized);
            }),
        m_RecentProjects.end());

    if (m_RecentProjects.size() != beforeSize)
        SaveRecentProjects();
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

bool EditorSettingsService::LoadInputActionSettings(Engine::InputActionSettings& out) const
{
    if (m_ProjectRoot.empty()) return false;

    Engine::InputActionSettings candidate;
    candidate.Maps.push_back({
    "Gameplay",
    {
        { "Jump", "Button" }, { "Sprint", "Button" },
        { "OrbitCamera", "Button" },
        { "MoveForward", "Button" }, { "MoveBackward", "Button" },
        { "MoveLeft", "Button" }, { "MoveRight", "Button" }
    },
    {
        { "Jump", "Space" }, { "Sprint", "LShift" }, { "Sprint", "RShift" },
        { "OrbitCamera", "Mouse:Right" },
        { "MoveForward", "W" }, { "MoveBackward", "S" },
        { "MoveLeft", "A" }, { "MoveRight", "D" }
    }
        });

    const auto path = m_ProjectRoot / "Config" / "InputActions.json";
    const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
    if (!ReadEditorSettingsFile(candidate, path, registry, Engine::InputManager::ValidateInputActionSettings, "InputActions"))
        return false;

    out = std::move(candidate);
    return true;
}

EEditorSettingsSaveStatus EditorSettingsService::SaveInputActionSettings(const Engine::InputActionSettings& settings) const
{
    if (m_ProjectRoot.empty() || !Engine::InputManager::ValidateInputActionSettings(settings))
        return EEditorSettingsSaveStatus::Failed;

    const auto path = m_ProjectRoot / "Config" / "InputActions.json";
    const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
    return WriteEditorSettingsFile(settings, path, registry, true, "InputActions");
}

EEditorSettingsSaveStatus EditorSettingsService::SaveProjectSettings(const Engine::ProjectSettings& settings)
{
    if (m_ProjectRoot.empty())
        return EEditorSettingsSaveStatus::Failed;
    const std::filesystem::path projectPath = m_ProjectRoot / "Config" / "Project.json";
    const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
    const auto result = WriteEditorSettingsFile(settings, projectPath, registry, true, "ProjectSettings");
    if (result == EEditorSettingsSaveStatus::Saved)
        m_ProjectSettings = settings;
    return result;
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

EEditorSettingsSaveStatus EditorSettingsService::SetBuildProfileWorkspace(const BuildProfileWorkspace& profile)
{
    m_EditorWorkspace.BuildProfile = profile;
    return SaveWorkspace();
}

void EditorSettingsService::RememberEditedScene(const std::filesystem::path& path)
{
    const std::string previousLastScene = m_EditorWorkspace.LastScenePath;
    if (path.empty())
    {
        m_EditorWorkspace.LastScenePath.clear();
        if (!previousLastScene.empty())
            SaveWorkspace();
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

    if (m_EditorWorkspace.LastScenePath != previousLastScene)
        SaveWorkspace();
}

void EditorSettingsService::BindRuntime(ImGuiManager& gui, TimeManager& time, LocalizationManager& localization, RHI& rhi)
{
    m_ImGuiManager = &gui;
    m_TimeManager = &time;
    m_LocalizationManager = &localization;
    m_RHI = &rhi;
    if (Engine::RuntimeSettingsManager::IsCreated())
        Engine::RuntimeSettingsManager::Get().BindRuntime(m_TimeManager, m_LocalizationManager, nullptr, m_RHI);
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
    m_ImGuiManager->ApplyTheme(preferences.Theme);
    m_TimeManager->SetTargetFPS(static_cast<uint32>(preferences.TargetFPS));
    m_ImGuiManager->ApplyCameraPreferences(preferences.CameraSpeed, preferences.CameraFOV);
    // 다른 설정을 적용할 때 뷰포트별로 조절한 거리를 되돌리지 않도록 값이 바뀐 경우에만 적용한다.
    if (preferences.CameraNearPlane != m_EditorPreferences.CameraNearPlane || preferences.CameraFarPlane != m_EditorPreferences.CameraFarPlane)
        m_ImGuiManager->ApplyCameraClipPlanes(preferences.CameraNearPlane, preferences.CameraFarPlane);

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

EditorSettingsChangeResult EditorSettingsService::SetEditorRaytracedShadowMode(std::int32_t mode)
{
    if (!Engine::IsValidRaytracedShadowMode(mode))
        return {};

    EditorPreferences candidate = m_EditorPreferences;
    candidate.RaytracedShadowMode = mode;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetEditorRaytracedReflectionMode(std::int32_t mode)
{
    if (!Engine::IsValidRaytracedShadowMode(mode))
        return {};

    EditorPreferences candidate = m_EditorPreferences;
    candidate.RaytracedReflectionMode = mode;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetEditorTemporalAAMode(std::int32_t mode)
{
    if (!Engine::IsValidRaytracedShadowMode(mode))
        return {};

    EditorPreferences candidate = m_EditorPreferences;
    candidate.TemporalAAMode = mode;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetEditorAmbientOcclusionMode(std::int32_t mode)
{
    if (!Engine::IsValidAmbientOcclusionMode(mode))
        return {};

    EditorPreferences candidate = m_EditorPreferences;
    candidate.AmbientOcclusionMode = mode;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetEditorProbeGIMode(std::int32_t mode)
{
    if (!Engine::IsValidRaytracedShadowMode(mode))
        return {};

    EditorPreferences candidate = m_EditorPreferences;
    candidate.ProbeGIMode = mode;
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

EditorSettingsChangeResult EditorSettingsService::SetGizmoSnapping(float translation, float rotation, float scale)
{
    EditorPreferences candidate = m_EditorPreferences;
    candidate.GizmoSnapTranslation = translation;
    candidate.GizmoSnapRotation = rotation;
    candidate.GizmoSnapScale = scale;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetGizmoSnapTranslation(float snap)
{
    EditorPreferences candidate = m_EditorPreferences;
    candidate.GizmoSnapTranslation = snap;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetGizmoSnapRotation(float snap)
{
    EditorPreferences candidate = m_EditorPreferences;
    candidate.GizmoSnapRotation = snap;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetGizmoSnapScale(float snap)
{
    EditorPreferences candidate = m_EditorPreferences;
    candidate.GizmoSnapScale = snap;
    return ApplyAndSavePreferences(candidate);
}

EditorSettingsChangeResult EditorSettingsService::SetEditorPreferences(const EditorPreferences& preferences)
{
    return ApplyAndSavePreferences(preferences);
}

EditorSettingsChangeResult EditorSettingsService::SetPreviewLightIntensity(const std::array<float, 3>& intensities)
{
    EditorPreferences candidate = m_EditorPreferences;
    candidate.PreviewLightIntensity = intensities;

    if (!candidate.HasValidValues())
        return {};

    m_EditorPreferences.PreviewLightIntensity = intensities;
    return { true, SavePreferences() };
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

bool EditorSettingsService::MatchesWindowRequest(const EditorWindowRequest& request, const EditorApplicationState& state)
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

void EditorSettingsService::FinishWindowChange(EEditorWindowRequestStatus status, const EditorApplicationState& state)
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

const EditorPreferences& EditorSettingsService::GetPreferences() const { return m_EditorPreferences; }
const EditorWorkspace& EditorSettingsService::GetWorkspace() const { return m_EditorWorkspace; }
const Engine::ProjectSettings& EditorSettingsService::GetProjectSettings() const { return m_ProjectSettings; }
const std::filesystem::path& EditorSettingsService::GetProjectRoot() const { return m_ProjectRoot; }
const std::filesystem::path& EditorSettingsService::GetPreferencesPath() const { return m_EditorPreferencesPath; }
const std::filesystem::path& EditorSettingsService::GetWorkspacePath() const { return m_EditorWorkspacePath; }

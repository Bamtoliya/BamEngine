#pragma once

#include "Editor_Includes.h"
#include "ProjectDescriptor.h"
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>

struct SDL_Window;

BEGIN(Editor)

struct ProjectSwitchResult
{
    bool Succeeded = false;
    bool Cancelled = false;
    bool FellBackToEmptyScene = false;
    std::string ErrorMessage;
    std::filesystem::path ProjectFilePath;
    std::filesystem::path ProjectRoot;
};

// Return true to proceed (discard unsaved edits / stop PIE), false to cancel switching.
using ConfirmProjectSwitchCallback = std::function<bool(bool hasUnsavedChanges, bool pieActive)>;

struct ProjectSwitchOptions
{
    bool StopPIEAutomatically = true;
    bool AllowUnsavedDiscardWithoutPrompt = false;
    ConfirmProjectSwitchCallback ConfirmCallback;
};

class ProjectSessionManager final : public Base
{
    DECLARE_SINGLETON(ProjectSessionManager)

#pragma region Constructor&Destructor
private:
    ProjectSessionManager() = default;
    ~ProjectSessionManager() override = default;
    EResult Initialize(void* arg = nullptr);
public:
    void Free() override;
#pragma endregion

#pragma region Project Lifecycle
public:
    ProjectSwitchResult CreateProject(const std::filesystem::path& targetDirectory, const std::string& projectName,
        Engine::EProjectTemplate projectTemplate = Engine::EProjectTemplate::Empty,
        bool switchImmediately = true, const ProjectSwitchOptions& options = {}, bool createCppModule = false);

    ProjectSwitchResult SwitchProject(const std::filesystem::path& bamProjectOrRoot,
        const ProjectSwitchOptions& options = {});

    void SyncFromCurrentPlatformPath();
#pragma endregion

#pragma region External Content Mounts
public:
    bool AddContentMount(const std::string& name, const std::filesystem::path& folderPath,
        bool readOnly = true, bool showUnderAssets = false, std::string* outError = nullptr);
    bool RemoveContentMount(std::string_view name, std::string* outError = nullptr);
    bool UpdateContentMount(const std::string& name, const std::filesystem::path& folderPath,
        bool readOnly, bool showUnderAssets, std::string* outError = nullptr);
    bool SetContentMountFlags(std::string_view name, bool readOnly, bool showUnderAssets,
        std::string* outError = nullptr);
#pragma endregion

#pragma region Queries & Hooks
public:
    using ProjectSwitchedCallback = std::function<void(const Engine::ProjectDescriptor&, const std::filesystem::path&)>;

    [[nodiscard]] const Engine::ProjectDescriptor& GetActiveDescriptor() const { return m_ActiveDescriptor; }
    [[nodiscard]] const std::filesystem::path& GetActiveProjectFilePath() const { return m_ActiveProjectFilePath; }
    [[nodiscard]] const std::filesystem::path& GetActiveProjectRoot() const { return m_ActiveProjectRoot; }
    [[nodiscard]] bool HasActiveProjectFile() const { return !m_ActiveProjectFilePath.empty(); }
    [[nodiscard]] bool IsSwitching() const { return m_IsSwitching; }
    [[nodiscard]] std::string GetActiveProjectName() const;

    void SetWindow(SDL_Window* window) { m_Window = window; }
    void SetOnProjectSwitched(ProjectSwitchedCallback callback) { m_OnProjectSwitched = std::move(callback); }
#pragma endregion

private:
    bool HasUnsavedEditorChanges() const;
    void QuiesceBackgroundWork();
    void ClearEditorStateAndPanels();
    void UnloadSceneAndProjectResources();
    void WaitForGPUIdle();
    void RebindPathsAndMounts(const std::filesystem::path& newProjectRoot,
        const std::filesystem::path& newAssetRoot, const Engine::ProjectDescriptor& descriptor);
    bool LoadStartupSceneOrFallback(const std::filesystem::path& newProjectRoot,
        const std::filesystem::path& newAssetRoot, bool& outFellBackToEmpty);
    bool PersistActiveDescriptor(std::string* outError);
    void RefreshContentBrowser(const std::filesystem::path& newAssetRoot, bool navigateToRoot);
    void UpdateWindowTitle() const;

private:
    Engine::ProjectDescriptor m_ActiveDescriptor;
    std::filesystem::path m_ActiveProjectFilePath;
    std::filesystem::path m_ActiveProjectRoot;
    SDL_Window* m_Window = nullptr;
    ProjectSwitchedCallback m_OnProjectSwitched;
    bool m_IsSwitching = false;
};

END

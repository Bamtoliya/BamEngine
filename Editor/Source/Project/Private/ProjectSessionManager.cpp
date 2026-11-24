#include "ProjectSessionManager.h"
#include "PlatformPath.h"
#include "EditorSettingsService.h"
#include "Application.h"
#include "ImGuiManager.h"
#include "ResourceEditorInterface.h"
#include "AsyncResourceOpenService.h"
#include "ContentBrowserPanel.h"
#include "PIEManager.h"
#include "RecoveryManager.h"
#include "HotReloadManager.h"
#include "SelectionManager.h"
#include "EntityClipboard.h"
#include "AssetCache.h"
#include "SceneManager.h"
#include "ResourceManager.h"
#include "AsyncResourceLoader.h"
#include "ResourceStreamer.h"
#include "Renderer.h"
#include "LocalizationManager.h"
#include "Logger.h"
#include <archive/JsonArchive.h>
#include <SDL3/SDL.h>

#ifdef _WIN32
#include "DirectX12RHI.h"
#endif

#include <algorithm>
#include <chrono>
#include <thread>

namespace
{
    std::string PathToUtf8String(const std::filesystem::path& path)
    {
        const auto u8 = Engine::PlatformPath::Normalize(path).generic_u8string();
        return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
    }

    std::string FormatMountPathForDescriptor(const std::filesystem::path& projectRoot, const std::filesystem::path& folderPath)
    {
        const std::filesystem::path normalizedFolder = Engine::PlatformPath::Normalize(folderPath);
        if (!projectRoot.empty())
        {
            std::error_code ec;
            const std::filesystem::path rel = std::filesystem::relative(normalizedFolder, projectRoot, ec);
            if (!ec && !rel.empty() && !rel.is_absolute())
            {
                const std::string relStr = rel.generic_string();
                // Keep relative path only when it stays reasonably close (not crossing drives or deep parent chains).
                if (!relStr.starts_with("../../.."))
                    return relStr;
            }
        }
        return PathToUtf8String(normalizedFolder);
    }

    bool IsValidMountAlias(std::string_view name)
    {
        if (name.empty())
            return false;
        for (char c : name)
        {
            if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                return false;
        }
        return name != "." && name != "..";
    }

    bool EqualsAsciiIgnoreCase(std::string_view lhs, std::string_view rhs)
    {
        if (lhs.size() != rhs.size())
            return false;
        for (size_t i = 0; i < lhs.size(); ++i)
        {
            char a = lhs[i];
            char b = rhs[i];
            if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = static_cast<char>(b - 'A' + 'a');
            if (a != b)
                return false;
        }
        return true;
    }

    std::filesystem::path ResolveCandidateScenePath(const std::string& rawPath,
        const std::filesystem::path& projectRoot, const std::filesystem::path& assetRoot)
    {
        if (rawPath.empty())
            return {};

        std::error_code ec;
        if (Engine::PlatformPath::IsVirtualUri(rawPath))
        {
            const std::filesystem::path resolved = Engine::PlatformPath::ResolveUri(rawPath);
            if (std::filesystem::is_regular_file(resolved, ec))
                return resolved;
        }

        const std::filesystem::path direct(std::filesystem::u8path(rawPath));
        if (direct.is_absolute())
        {
            if (std::filesystem::is_regular_file(direct, ec))
                return Engine::PlatformPath::Normalize(direct);
            return {};
        }

        const std::filesystem::path underAsset = Engine::PlatformPath::Normalize(assetRoot / direct);
        if (std::filesystem::is_regular_file(underAsset, ec))
            return underAsset;

        const std::filesystem::path underProject = Engine::PlatformPath::Normalize(projectRoot / direct);
        if (std::filesystem::is_regular_file(underProject, ec))
            return underProject;

        return {};
    }
}

IMPLEMENT_SINGLETON(ProjectSessionManager)

#pragma region Constructor&Destructor
EResult ProjectSessionManager::Initialize(void*)
{
    SyncFromCurrentPlatformPath();
    return EResult::Success;
}

void ProjectSessionManager::Free()
{
    m_OnProjectSwitched = nullptr;
    m_Window = nullptr;
    m_IsSwitching = false;
    __super::Free();
}
#pragma endregion

void ProjectSessionManager::SyncFromCurrentPlatformPath()
{
    const std::filesystem::path currentRoot = Engine::PlatformPath::GetProjectRoot();
    if (currentRoot.empty())
        return;

    m_ActiveProjectRoot = Engine::PlatformPath::Normalize(currentRoot);
    const std::filesystem::path foundFile = Engine::ProjectDescriptor::FindProjectFile(m_ActiveProjectRoot);
    if (!foundFile.empty())
    {
        Engine::ProjectDescriptor desc;
        if (Engine::ProjectDescriptor::LoadFromFile(foundFile, desc))
        {
            m_ActiveDescriptor = std::move(desc);
            m_ActiveProjectFilePath = foundFile;
            Engine::PlatformPath::ClearContentMounts();
            for (const auto& mount : m_ActiveDescriptor.ResolveContentMounts(m_ActiveProjectRoot))
                Engine::PlatformPath::RegisterContentMount(mount);
            return;
        }
    }

    m_ActiveProjectFilePath.clear();
    m_ActiveDescriptor = {};
    if (EditorSettingsService::IsCreated() && !EditorSettingsService::Get().GetProjectSettings().ProjectName.empty())
        m_ActiveDescriptor.ProjectName = EditorSettingsService::Get().GetProjectSettings().ProjectName;
    else
        m_ActiveDescriptor.ProjectName = m_ActiveProjectRoot.filename().string();
}

std::string ProjectSessionManager::GetActiveProjectName() const
{
    if (!m_ActiveDescriptor.ProjectName.empty())
        return m_ActiveDescriptor.ProjectName;
    if (!m_ActiveProjectRoot.empty())
        return m_ActiveProjectRoot.filename().string();
    return "Untitled";
}

ProjectSwitchResult ProjectSessionManager::CreateProject(const std::filesystem::path& targetDirectory,
    const std::string& projectName, Engine::EProjectTemplate projectTemplate,
    bool switchImmediately, const ProjectSwitchOptions& options)
{
    ProjectSwitchResult result;
    if (targetDirectory.empty() || projectName.empty())
    {
        result.ErrorMessage = "Target directory and project name must not be empty.";
        return result;
    }

    std::filesystem::path createdProjectFile;
    std::string scaffoldError;
    if (!Engine::ProjectDescriptor::ScaffoldNewProject(
            targetDirectory, projectName, &createdProjectFile, &scaffoldError, projectTemplate))
    {
        result.ErrorMessage = scaffoldError.empty() ? "Failed to scaffold new project." : scaffoldError;
        return result;
    }

    if (!switchImmediately)
    {
        result.Succeeded = true;
        result.ProjectFilePath = createdProjectFile;
        result.ProjectRoot = Engine::PlatformPath::Normalize(createdProjectFile.parent_path());
        if (EditorSettingsService::IsCreated())
            EditorSettingsService::Get().RecordRecentProject(projectName, createdProjectFile);
        return result;
    }

    return SwitchProject(createdProjectFile, options);
}

ProjectSwitchResult ProjectSessionManager::SwitchProject(const std::filesystem::path& bamProjectOrRoot,
    const ProjectSwitchOptions& options)
{
    ProjectSwitchResult result;
    if (m_IsSwitching)
    {
        result.ErrorMessage = "A project switch is already in progress.";
        return result;
    }

    // -------------------------------------------------------------------------
    // Step 1: Pre-validation & user confirmation (cancellable; touches no state)
    // -------------------------------------------------------------------------
    if (bamProjectOrRoot.empty())
    {
        result.ErrorMessage = "Project path is empty.";
        return result;
    }

    std::error_code ec;
    const std::filesystem::path normalizedInput = Engine::PlatformPath::Normalize(bamProjectOrRoot);
    const std::filesystem::path projectFile = Engine::ProjectDescriptor::FindProjectFile(normalizedInput);

    Engine::ProjectDescriptor candidateDescriptor;
    std::filesystem::path newProjectRoot;

    if (!projectFile.empty())
    {
        std::string loadError;
        if (!Engine::ProjectDescriptor::LoadFromFile(projectFile, candidateDescriptor, &loadError))
        {
            result.ErrorMessage = loadError.empty() ? "Failed to load .bamproject descriptor." : loadError;
            return result;
        }
        newProjectRoot = Engine::PlatformPath::Normalize(projectFile.parent_path());
    }
    else if (std::filesystem::is_directory(normalizedInput, ec) &&
             std::filesystem::exists(normalizedInput / "Config" / "Project.json", ec))
    {
        // Legacy single-root project directory fallback
        newProjectRoot = normalizedInput;
        candidateDescriptor.ProjectName = newProjectRoot.filename().string();
        candidateDescriptor.AssetDirectory = "Assets";
    }
    else
    {
        result.ErrorMessage = "Target path is neither a valid .bamproject file nor a project directory with Config/Project.json.";
        return result;
    }

    if (!std::filesystem::is_directory(newProjectRoot, ec))
    {
        result.ErrorMessage = "Target project root directory does not exist.";
        return result;
    }

    const std::filesystem::path newAssetRoot = candidateDescriptor.ResolveAssetRoot(newProjectRoot);
    if (!std::filesystem::exists(newAssetRoot, ec))
    {
        std::filesystem::create_directories(newAssetRoot, ec);
        if (ec)
        {
            result.ErrorMessage = "Target project AssetDirectory is not accessible: " + ec.message();
            return result;
        }
    }

    const bool pieActive = PIEManager::IsCreated() && !PIEManager::Get().IsEditing();
    const bool hasUnsaved = HasUnsavedEditorChanges();

    if (hasUnsaved || pieActive)
    {
        if (options.ConfirmCallback)
        {
            if (!options.ConfirmCallback(hasUnsaved, pieActive))
            {
                result.Cancelled = true;
                return result;
            }
        }
        else if (hasUnsaved && !options.AllowUnsavedDiscardWithoutPrompt)
        {
            result.Cancelled = true;
            result.ErrorMessage = "Unsaved changes must be confirmed before switching projects.";
            return result;
        }
    }

    if (pieActive)
    {
        if (options.StopPIEAutomatically)
        {
            PIEManager::Get().Stop();
        }
        else
        {
            result.Cancelled = true;
            result.ErrorMessage = "Play-In-Editor must be stopped before switching projects.";
            return result;
        }
    }

    m_IsSwitching = true;
    struct SwitchGuard { bool& Flag; ~SwitchGuard() { Flag = false; } } guard{ m_IsSwitching };

    BAM_LOG(Info, "Editor", "Project", "Switching active project to \"{}\" ({})",
        candidateDescriptor.ProjectName, PathToUtf8String(newProjectRoot));

    // -------------------------------------------------------------------------
    // Step 2: Quiesce background queues & recovery session
    // -------------------------------------------------------------------------
    QuiesceBackgroundWork();

    // -------------------------------------------------------------------------
    // Step 3: Clear editor state, history, selection, clipboard, and caches
    // -------------------------------------------------------------------------
    ClearEditorStateAndPanels();

    // -------------------------------------------------------------------------
    // Step 4: Unload scene, wait for GPU idle, unload project/mount resources
    // -------------------------------------------------------------------------
    UnloadSceneAndProjectResources();

    // -------------------------------------------------------------------------
    // Step 5: Rebind PlatformPath roots, mounts, cache/Temp/Logs directories
    // -------------------------------------------------------------------------
    RebindPathsAndMounts(newProjectRoot, newAssetRoot, candidateDescriptor);

    // -------------------------------------------------------------------------
    // Step 6: Reload EditorSettingsService & project localization
    // -------------------------------------------------------------------------
    if (EditorSettingsService::IsCreated())
    {
        EditorSettingsService::Get().ReloadProjectSettings(newProjectRoot);
    }
    if (Engine::LocalizationManager::IsCreated())
    {
        Engine::LocalizationManager::Get().ReloadProjectData();
    }

    // -------------------------------------------------------------------------
    // Step 7: Rebind C++ project module/hot-reload, restart recovery, refresh browser, load startup scene
    // -------------------------------------------------------------------------
    if (HotReloadManager::IsCreated())
    {
        HotReloadManager::Get().RebindForProject(newProjectRoot, candidateDescriptor);
    }

    if (RecoveryManager::IsCreated())
    {
        RecoveryManagerDesc recoveryDesc{ newProjectRoot };
        RecoveryManager::Get().Initialize(&recoveryDesc);
    }

    RefreshContentBrowser(newAssetRoot, true);

    bool fellBackToEmpty = false;
    LoadStartupSceneOrFallback(newProjectRoot, newAssetRoot, fellBackToEmpty);

    // -------------------------------------------------------------------------
    // Step 8: Finalize active descriptor, window title, and recent projects
    // -------------------------------------------------------------------------
    m_ActiveDescriptor = std::move(candidateDescriptor);
    m_ActiveProjectFilePath = projectFile;
    m_ActiveProjectRoot = newProjectRoot;

    UpdateWindowTitle();

    if (EditorSettingsService::IsCreated())
    {
        const std::filesystem::path recentTarget = !m_ActiveProjectFilePath.empty() ? m_ActiveProjectFilePath : m_ActiveProjectRoot;
        EditorSettingsService::Get().RecordRecentProject(m_ActiveDescriptor.ProjectName, recentTarget);
    }

    if (m_OnProjectSwitched)
    {
        m_OnProjectSwitched(m_ActiveDescriptor, m_ActiveProjectRoot);
    }

    result.Succeeded = true;
    result.FellBackToEmptyScene = fellBackToEmpty;
    result.ProjectFilePath = m_ActiveProjectFilePath;
    result.ProjectRoot = m_ActiveProjectRoot;

    BAM_LOG(Info, "Editor", "Project", "Project switch complete: \"{}\" (fallbackEmptyScene={})",
        m_ActiveDescriptor.ProjectName, fellBackToEmpty ? "true" : "false");
    return result;
}

bool ProjectSessionManager::HasUnsavedEditorChanges() const
{
    if (Application::IsCreated())
    {
        auto& app = Application::Get();
        if (!app.GetDocuments().GetModifiedDocumentIds().empty())
            return true;
        if (app.GetEditHistory().HasUnsavedChanges())
            return true;
    }

    if (ImGuiManager::IsCreated())
    {
        for (IImGui* panel : ImGuiManager::Get().GetImGuiPanels())
        {
            if (auto* resEditor = dynamic_cast<ResourceEditorInterface*>(panel))
            {
                if (resEditor->IsOpen() && resEditor->HasUnsavedResourceChanges())
                    return true;
            }
        }
    }

    return false;
}

void ProjectSessionManager::QuiesceBackgroundWork()
{
    if (ImGuiManager::IsCreated())
    {
        for (IImGui* panel : ImGuiManager::Get().GetImGuiPanels())
        {
            if (auto* resEditor = dynamic_cast<ResourceEditorInterface*>(panel))
            {
                AsyncResourceOpenService::Get().CancelRequestsForEditor(resEditor);
                resEditor->CancelAsyncOpen();
            }
        }
    }

    // Drain bounded pending async resource loads before unloading project resources.
    if (Engine::AsyncResourceLoader::IsCreated())
    {
        auto& loader = Engine::AsyncResourceLoader::Get();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        while (loader.GetPendingCount() > 0 && std::chrono::steady_clock::now() < deadline)
        {
            loader.StepMainThread(16.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    if (Engine::ResourceStreamer::IsCreated())
    {
        auto& streamer = Engine::ResourceStreamer::Get();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
        while (streamer.IsBusy() && std::chrono::steady_clock::now() < deadline)
        {
            streamer.Update(16.0);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Cleanly close current project's recovery session lock and stop module watchers/builds before switching project root.
    if (HotReloadManager::IsCreated())
    {
        HotReloadManager::Get().QuiesceProjectHotReload();
    }
    if (RecoveryManager::IsCreated())
    {
        RecoveryManager::Get().OnShutdown();
    }
}

void ProjectSessionManager::ClearEditorStateAndPanels()
{
    if (ImGuiManager::IsCreated())
    {
        for (IImGui* panel : ImGuiManager::Get().GetImGuiPanels())
        {
            if (auto* resEditor = dynamic_cast<ResourceEditorInterface*>(panel))
            {
                if (resEditor->IsOpen())
                    resEditor->SetOpen(false);
            }
        }
    }

    if (Application::IsCreated())
    {
        Application::Get().GetEditHistory().Clear();
    }

    EntityClipboard::Clear();

    if (SelectionManager::IsCreated())
    {
        auto& selection = SelectionManager::Get();
        selection.ClearSelection(true);
        selection.SetSelectedAsset({});
        selection.SetSelectedResource({});
    }

    if (AssetCache::IsCreated())
    {
        AssetCache::Get().ClearAll();
    }
}

void ProjectSessionManager::UnloadSceneAndProjectResources()
{
    if (Engine::SceneManager::IsCreated())
    {
        auto& sceneMgr = Engine::SceneManager::Get();
        sceneMgr.UnloadAllAdditiveScenes();
        sceneMgr.CloseScene();
    }

    if (Engine::Renderer::IsCreated())
    {
        Engine::Renderer::Get().ClearAllRenderQueues();
    }

    WaitForGPUIdle();

    if (Engine::ResourceManager::IsCreated())
    {
        Engine::ResourceManager::Get().UnloadProjectScopedResources();
    }

    // Unload active project's C++ module DLL only after scene unload and GPU idle to avoid dangling pointers.
    if (HotReloadManager::IsCreated())
    {
        HotReloadManager::Get().UnloadProjectModule();
    }
}

void ProjectSessionManager::WaitForGPUIdle()
{
    if (!Engine::Renderer::IsCreated())
        return;

    auto& renderer = Engine::Renderer::Get();
    Engine::RHI* rhi = renderer.GetRHI();
    if (!rhi)
        return;

    switch (renderer.GetRHIType())
    {
#ifdef _WIN32
    case Engine::ERHIType::DirectX12:
        if (auto* dxRhi = dynamic_cast<Engine::DirectX12RHI*>(rhi))
            dxRhi->WaitForGPU();
        break;
#endif
    case Engine::ERHIType::SDLGPU:
        if (auto* device = reinterpret_cast<SDL_GPUDevice*>(rhi->GetNativeRHI()))
            SDL_WaitForGPUIdle(device);
        break;
    default:
        break;
    }
}

void ProjectSessionManager::RebindPathsAndMounts(const std::filesystem::path& newProjectRoot,
    const std::filesystem::path& newAssetRoot, const Engine::ProjectDescriptor& descriptor)
{
    // Lock EngineRoot / EngineResourceRoot / EditorResourceRoot before rebinding ProjectRoot
    // so Engine:// and Editor:// remain anchored to the engine installation.
    const std::filesystem::path engineRoot = Engine::PlatformPath::GetEngineRoot();
    if (!engineRoot.empty())
        Engine::PlatformPath::SetEngineRoot(engineRoot);

    const std::filesystem::path engineResRoot = Engine::PlatformPath::GetEngineResourceRoot();
    if (!engineResRoot.empty())
        Engine::PlatformPath::SetEngineResourceRoot(engineResRoot);

    const std::filesystem::path editorResRoot = Engine::PlatformPath::GetEditorResourceRoot();
    if (!editorResRoot.empty())
        Engine::PlatformPath::SetEditorResourceRoot(editorResRoot);

    Engine::PlatformPath::SetProjectRoot(newProjectRoot);
    Engine::PlatformPath::SetProjectAssetRoot(newAssetRoot);

    Engine::PlatformPath::ClearContentMounts();
    for (const auto& mount : descriptor.ResolveContentMounts(newProjectRoot))
    {
        Engine::PlatformPath::RegisterContentMount(mount);
    }

    std::error_code ec;
    std::filesystem::create_directories(newProjectRoot / ".bam" / "cache", ec);
    std::filesystem::create_directories(newProjectRoot / "Temp", ec);
    std::filesystem::create_directories(newProjectRoot / "Logs", ec);
}

bool ProjectSessionManager::LoadStartupSceneOrFallback(const std::filesystem::path& newProjectRoot,
    const std::filesystem::path& newAssetRoot, bool& outFellBackToEmpty)
{
    outFellBackToEmpty = false;
    if (!Engine::SceneManager::IsCreated())
        return true;

    std::filesystem::path sceneToLoad;
    if (EditorSettingsService::IsCreated())
    {
        const auto& settings = EditorSettingsService::Get();
        sceneToLoad = ResolveCandidateScenePath(settings.GetWorkspace().LastScenePath, newProjectRoot, newAssetRoot);
        if (sceneToLoad.empty())
            sceneToLoad = ResolveCandidateScenePath(settings.GetProjectSettings().StartupScenePath, newProjectRoot, newAssetRoot);
    }
    if (sceneToLoad.empty())
    {
        sceneToLoad = ResolveCandidateScenePath("Scenes/Main.bamscene.json", newProjectRoot, newAssetRoot);
    }

    if (!sceneToLoad.empty())
    {
        archive::JsonArchive reader;
        if (reader.LoadFromFile(sceneToLoad) &&
            Engine::SceneManager::Get().LoadScene(reader, sceneToLoad.wstring()) == EResult::Success)
        {
            if (Application::IsCreated())
                Application::Get().AdoptSceneDocument(sceneToLoad);
            return true;
        }

        BAM_LOG(Warn, "Editor", "Project", "Failed to load startup scene ({}); falling back to empty scene.",
            PathToUtf8String(sceneToLoad));
    }

    // Safe fallback: always leave editor with a valid empty scene instead of a broken half-loaded state.
    Engine::SceneManager::Get().NewScene();
    if (Application::IsCreated())
        Application::Get().AdoptSceneDocument({});
    outFellBackToEmpty = true;
    return false;
}

bool ProjectSessionManager::AddContentMount(const std::string& name, const std::filesystem::path& folderPath,
    bool readOnly, bool showUnderAssets, std::string* outError)
{
    if (!IsValidMountAlias(name))
    {
        if (outError) *outError = "Invalid mount alias name.";
        return false;
    }

    std::error_code ec;
    const std::filesystem::path normalizedFolder = Engine::PlatformPath::Normalize(folderPath);
    if (normalizedFolder.empty() || !std::filesystem::is_directory(normalizedFolder, ec))
    {
        if (outError) *outError = "Mount target folder does not exist.";
        return false;
    }

    for (const auto& existing : m_ActiveDescriptor.ContentMounts)
    {
        if (EqualsAsciiIgnoreCase(existing.Name, name))
        {
            if (outError) *outError = "A content mount with that name already exists.";
            return false;
        }
    }

    Engine::ContentMountDesc runtimeDesc;
    runtimeDesc.Name = name;
    runtimeDesc.RootPath = normalizedFolder;
    runtimeDesc.ReadOnly = readOnly;
    runtimeDesc.ShowUnderAssets = showUnderAssets;

    if (!Engine::PlatformPath::RegisterContentMount(runtimeDesc))
    {
        if (outError) *outError = "Failed to register content mount in PlatformPath.";
        return false;
    }

    Engine::ProjectMountEntry entry;
    entry.Name = name;
    entry.Path = FormatMountPathForDescriptor(m_ActiveProjectRoot, normalizedFolder);
    entry.ReadOnly = readOnly;
    entry.ShowUnderAssets = showUnderAssets;
    m_ActiveDescriptor.ContentMounts.push_back(std::move(entry));

    if (!PersistActiveDescriptor(outError))
    {
        m_ActiveDescriptor.ContentMounts.pop_back();
        Engine::PlatformPath::UnregisterContentMount(name);
        return false;
    }

    RefreshContentBrowser({}, false);
    return true;
}

bool ProjectSessionManager::RemoveContentMount(std::string_view name, std::string* outError)
{
    auto it = std::find_if(m_ActiveDescriptor.ContentMounts.begin(), m_ActiveDescriptor.ContentMounts.end(),
        [name](const Engine::ProjectMountEntry& entry) { return EqualsAsciiIgnoreCase(entry.Name, name); });

    if (it == m_ActiveDescriptor.ContentMounts.end())
    {
        if (Engine::PlatformPath::HasContentMount(name))
        {
            Engine::PlatformPath::UnregisterContentMount(name);
            RefreshContentBrowser({}, false);
            return true;
        }
        if (outError) *outError = "Content mount not found.";
        return false;
    }

    const Engine::ProjectMountEntry backup = *it;
    m_ActiveDescriptor.ContentMounts.erase(it);
    Engine::PlatformPath::UnregisterContentMount(backup.Name);

    if (!PersistActiveDescriptor(outError))
    {
        m_ActiveDescriptor.ContentMounts.push_back(backup);
        for (const auto& mount : m_ActiveDescriptor.ResolveContentMounts(m_ActiveProjectRoot))
        {
            if (EqualsAsciiIgnoreCase(mount.Name, backup.Name))
                Engine::PlatformPath::RegisterContentMount(mount);
        }
        return false;
    }

    RefreshContentBrowser({}, false);
    return true;
}

bool ProjectSessionManager::UpdateContentMount(const std::string& name, const std::filesystem::path& folderPath,
    bool readOnly, bool showUnderAssets, std::string* outError)
{
    auto it = std::find_if(m_ActiveDescriptor.ContentMounts.begin(), m_ActiveDescriptor.ContentMounts.end(),
        [&name](const Engine::ProjectMountEntry& entry) { return EqualsAsciiIgnoreCase(entry.Name, name); });
    if (it == m_ActiveDescriptor.ContentMounts.end())
    {
        if (outError) *outError = "Content mount not found.";
        return false;
    }

    std::error_code ec;
    const std::filesystem::path normalizedFolder = Engine::PlatformPath::Normalize(folderPath);
    if (normalizedFolder.empty() || !std::filesystem::is_directory(normalizedFolder, ec))
    {
        if (outError) *outError = "Mount target folder does not exist.";
        return false;
    }

    const Engine::ProjectMountEntry backup = *it;
    it->Path = FormatMountPathForDescriptor(m_ActiveProjectRoot, normalizedFolder);
    it->ReadOnly = readOnly;
    it->ShowUnderAssets = showUnderAssets;

    Engine::ContentMountDesc runtimeDesc;
    runtimeDesc.Name = it->Name;
    runtimeDesc.RootPath = normalizedFolder;
    runtimeDesc.ReadOnly = readOnly;
    runtimeDesc.ShowUnderAssets = showUnderAssets;

    if (!Engine::PlatformPath::RegisterContentMount(runtimeDesc))
    {
        *it = backup;
        if (outError) *outError = "Failed to update content mount in PlatformPath.";
        return false;
    }

    if (!PersistActiveDescriptor(outError))
    {
        *it = backup;
        for (const auto& mount : m_ActiveDescriptor.ResolveContentMounts(m_ActiveProjectRoot))
        {
            if (EqualsAsciiIgnoreCase(mount.Name, backup.Name))
                Engine::PlatformPath::RegisterContentMount(mount);
        }
        return false;
    }

    RefreshContentBrowser({}, false);
    return true;
}

bool ProjectSessionManager::SetContentMountFlags(std::string_view name, bool readOnly, bool showUnderAssets,
    std::string* outError)
{
    for (const auto& mount : Engine::PlatformPath::GetContentMounts())
    {
        if (EqualsAsciiIgnoreCase(mount.Name, name))
            return UpdateContentMount(mount.Name, mount.RootPath, readOnly, showUnderAssets, outError);
    }

    if (outError) *outError = "Content mount not found.";
    return false;
}

bool ProjectSessionManager::PersistActiveDescriptor(std::string* outError)
{
    if (m_ActiveProjectFilePath.empty())
    {
        if (m_ActiveProjectRoot.empty())
            return true;
        const std::string safeName = GetActiveProjectName().empty() ? "Project" : GetActiveProjectName();
        m_ActiveProjectFilePath = m_ActiveProjectRoot / (safeName + ".bamproject");
    }

    return Engine::ProjectDescriptor::SaveToFile(m_ActiveProjectFilePath, m_ActiveDescriptor, outError);
}

void ProjectSessionManager::RefreshContentBrowser(const std::filesystem::path& newAssetRoot, bool navigateToRoot)
{
    if (!ImGuiManager::IsCreated())
        return;

    for (IImGui* panel : ImGuiManager::Get().GetImGuiPanels())
    {
        if (auto* cbPanel = dynamic_cast<ContentBrowserPanel*>(panel))
        {
            if (navigateToRoot && !newAssetRoot.empty())
                cbPanel->NavigateTo(newAssetRoot);
            cbPanel->RequestRefresh();
        }
    }
}

void ProjectSessionManager::UpdateWindowTitle() const
{
    if (!m_Window)
        return;

    const std::string title = "BamEngine Editor - " + GetActiveProjectName();
    SDL_SetWindowTitle(m_Window, title.c_str());
}

#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "AssetHotReloadManager.h"
#include "ModuleBuildManager.h"
#include "PlatformPath.h"
#include "ProjectDescriptor.h"
#include "Functions.h"
#include "ImGuiManager.h"
#include "PluginResourceEditor.h"

static std::filesystem::path ResolveProjectRoot()
{
    return Engine::PlatformPath::GetProjectRoot();
}
static std::filesystem::path ResolveBuildDirectory(const std::filesystem::path& projectRoot)
{
    std::error_code ec;
    const std::filesystem::path exeDir = Engine::PlatformPath::GetExecutableDirectory();
    // 1. <buildDir>/bin/<Config>/Editor.exe 구조 역추적 (Visual Studio 멀티 구성 프리셋)
    const std::filesystem::path candidate1 = exeDir.parent_path().parent_path();
    if (std::filesystem::exists(candidate1 / "CMakeCache.txt", ec))
    {
        return candidate1;
    }
    // 2. <buildDir>/bin/Editor.exe 구조 역추적 (Ninja 등 단일 구성 프리셋)
    const std::filesystem::path candidate2 = exeDir.parent_path();
    if (std::filesystem::exists(candidate2 / "CMakeCache.txt", ec))
    {
        return candidate2;
    }
    // 3. 빌드 프리셋 기본 경로 Fallback
#ifdef _DEBUG
    return projectRoot / "out/build/x64-debug";
#else
    return projectRoot / "out/build/x64-release";
#endif
}

IMPLEMENT_SINGLETON(HotReloadManager)
#pragma region Constructor&Destructor
EResult HotReloadManager::Initialize(void*)
{
    m_ModuleReloader = ModuleHotReloadManager::Create();
    if (!m_ModuleReloader) return EResult::Fail;
    m_ShaderReloader = ShaderHotReloadManager::Create();
    if (!m_ShaderReloader) { Free(); return EResult::Fail; }
    m_AssetReloader = AssetHotReloadManager::Create();
    if (!m_AssetReloader) { Free(); return EResult::Fail; }
    // 실제 엔진/프로젝트 루트와 CMake 빌드 디렉터리를 찾아 ModuleBuildManager 생성
    const std::filesystem::path engineRoot = Engine::PlatformPath::GetEngineRoot().empty()
        ? ResolveProjectRoot() : Engine::PlatformPath::GetEngineRoot();
    const std::filesystem::path projectRoot = engineRoot;
    const std::filesystem::path buildDir = ResolveBuildDirectory(engineRoot);
    m_ModuleBuilder = ModuleBuildManager::Create(projectRoot, buildDir);
    if (!m_ModuleBuilder) { Free(); return EResult::Fail; }
    m_ModuleBuilder->SetTriggersGameModuleReload(true);
    std::error_code ec;
    if (ImGuiManager::IsCreated() && std::filesystem::is_regular_file(buildDir / "CMakeCache.txt", ec))
    {
        const auto addBuilder = [this, &projectRoot, &buildDir](const std::string& target,
            const std::filesystem::path& relativeSource) -> ModuleBuildManager*
            {
                if (m_PluginBuilders.contains(target))
                    return nullptr;

                const auto source = ResolvePluginSourceDirectory(projectRoot, relativeSource);
                if (source.empty())
                    return nullptr;

                auto* builder = ModuleBuildManager::Create(
                    projectRoot, buildDir, source, std::wstring(target.begin(), target.end()));
                if (builder)
                    m_PluginBuilders.emplace(target, builder);

                return builder;
            };

        auto& gui = ImGuiManager::Get();
        for (const auto* panel : gui.GetPluginResourceEditors())
        {
            const auto& target = panel->GetTarget();
            auto* builder = addBuilder(target, panel->GetSourceDirectory());
            if (!builder)
                continue;

            builder->SetBuildCompletedCallback([target](bool success, const std::string&)
                {
                    if (!success || !ImGuiManager::IsCreated())
                        return;
                    if (auto* editor = ImGuiManager::Get().GetPluginResourceEditor(target))
                        editor->RequestReload();
                });
        }

        const auto& host = gui.GetPluginHost();
        for (std::size_t index = 0; index < host.GetCount(); ++index)
        {
            const auto& item = host.GetEntry(index);
            if (!item.Development)
                continue;

            const auto& development = *item.Development;
            auto* builder = addBuilder(development.Target, development.SourceDirectory);
            if (!builder)
                continue;

            builder->SetAutoCompileEnabled(false);
            builder->SetBuildCompletedCallback([index](bool success, const std::string&)
                {
                    if (!success || !ImGuiManager::IsCreated())
                        return;

                    auto& pluginHost = ImGuiManager::Get().GetPluginHost();
                    if (index < pluginHost.GetCount() && pluginHost.GetStatus(index).IsLoaded)
                        pluginHost.Request(index, EPluginHostAction::Reload);
                });
        }
    }
    return EResult::Success;
}

void HotReloadManager::Free()
{
    for (auto& [target, builder] : m_PluginBuilders)
        Safe_Release(builder);
    m_PluginBuilders.clear();
    Safe_Release(m_AssetReloader);
    Safe_Release(m_ShaderReloader);
    Safe_Release(m_ModuleReloader);
    Safe_Release(m_ModuleBuilder);

    __super::Free();
}
#pragma endregion

void HotReloadManager::Update()
{
    IHotReloader* reloaders[] = { m_ModuleReloader, m_ShaderReloader, m_AssetReloader, m_ModuleBuilder };
    for (IHotReloader* reloader : reloaders)
    {
        if (reloader) reloader->Update();
    }
    for (const auto& [target, builder] : m_PluginBuilders)
        builder->Update();
}

void HotReloadManager::QuiesceProjectHotReload()
{
    if (m_ModuleBuilder)
        m_ModuleBuilder->Quiesce();
    if (m_ModuleReloader)
        m_ModuleReloader->Quiesce();
}

void HotReloadManager::UnloadProjectModule()
{
    if (m_ModuleReloader)
        m_ModuleReloader->UnloadAndStopWatching();
}

EResult HotReloadManager::RebindForProject(const std::filesystem::path& projectRoot,
    const Engine::ProjectDescriptor& descriptor)
{
    const std::filesystem::path normProjectRoot = Engine::PlatformPath::Normalize(projectRoot);
    const std::filesystem::path normEngineRoot = Engine::PlatformPath::Normalize(Engine::PlatformPath::GetEngineRoot());

    if (m_ModuleReloader)
    {
        if (m_ModuleReloader->IsLoaded())
            m_ModuleReloader->UnloadAndStopWatching();
        if (!normProjectRoot.empty())
            m_ModuleReloader->SetShadowDirectory(normProjectRoot / "Temp" / "HotReload");
    }

    std::error_code ec;
    if (const auto runtimeMod = descriptor.ResolvePrimaryRuntimeModule(normProjectRoot))
    {
        const std::wstring targetW = Engine::StrToWStr(runtimeMod->Name);
        if (m_ModuleBuilder)
        {
            m_ModuleBuilder->SetTriggersGameModuleReload(true);
            m_ModuleBuilder->Reconfigure(normProjectRoot, runtimeMod->BuildDirectory, runtimeMod->SourceDirectory,
                targetW, runtimeMod->CMakeSourceRoot, runtimeMod->BinaryPath.parent_path());
        }

        if (m_ModuleReloader)
        {
            if (std::filesystem::is_regular_file(runtimeMod->BinaryPath, ec))
            {
                const EResult loadRes = m_ModuleReloader->LoadModule(runtimeMod->BinaryPath);
                if (loadRes == EResult::Success)
                    m_ModuleReloader->NotifyPostReloadCallbacks();
                return loadRes;
            }
            m_ModuleReloader->PrepareWatchForUnbuiltModule(runtimeMod->BinaryPath);
            EDITOR_LOG_INFO("[HotReload] Project module '{}' DLL not built yet; watching: {}",
                runtimeMod->Name, Engine::WStrToStr(runtimeMod->BinaryPath.wstring()));
        }
        return EResult::Success;
    }

    // Legacy single-root workspace fallback: projectRoot == engineRoot -> built-in GameModule
    const bool isEngineRootProject = !normEngineRoot.empty() &&
        (normProjectRoot.empty() || normProjectRoot == normEngineRoot);
    if (isEngineRootProject)
    {
        const std::filesystem::path exeDir = Engine::PlatformPath::GetExecutableDirectory();
        const std::filesystem::path engineBuildDir = ResolveBuildDirectory(normEngineRoot);
        if (m_ModuleBuilder)
        {
            m_ModuleBuilder->SetTriggersGameModuleReload(true);
            m_ModuleBuilder->Reconfigure(normEngineRoot, engineBuildDir,
                normEngineRoot / "Projects" / "GameModule" / "Source", L"GameModule", {}, exeDir);
        }

        const std::filesystem::path legacyDll = exeDir / L"GameModule.dll";
        if (m_ModuleReloader)
        {
            if (std::filesystem::is_regular_file(legacyDll, ec))
            {
                const EResult loadRes = m_ModuleReloader->LoadModule(legacyDll);
                if (loadRes == EResult::Success)
                    m_ModuleReloader->NotifyPostReloadCallbacks();
                return loadRes;
            }
            m_ModuleReloader->PrepareWatchForUnbuiltModule(legacyDll);
            EDITOR_LOG_WARN("[HotReload] GameModule.dll not found at: {}", legacyDll.string());
        }
        return EResult::Success;
    }

    // Independent content-only project (no C++ modules declared)
    if (m_ModuleBuilder)
    {
        m_ModuleBuilder->SetTriggersGameModuleReload(true);
        m_ModuleBuilder->Reconfigure(normProjectRoot, {}, {}, L"", {}, {});
    }
    if (m_ModuleReloader)
        m_ModuleReloader->UnloadAndStopWatching();

    EDITOR_LOG_INFO("[HotReload] Active project \"{}\" declares no C++ runtime modules.", descriptor.ProjectName);
    return EResult::Success;
}

#pragma region Getter
ModuleHotReloadManager& HotReloadManager::GetModuleReloader()
{
    assert(m_ModuleReloader != nullptr);
    return *m_ModuleReloader;
}

ShaderHotReloadManager& HotReloadManager::GetShaderReloader()
{
    assert(m_ShaderReloader != nullptr);
    return *m_ShaderReloader;
}

AssetHotReloadManager& HotReloadManager::GetAssetReloader()
{
    assert(m_AssetReloader != nullptr);
    return *m_AssetReloader;
}

ModuleBuildManager& HotReloadManager::GetModuleBuilder()
{
    assert(m_ModuleBuilder != nullptr);
    return *m_ModuleBuilder;
}
#pragma endregion

ModuleBuildManager* HotReloadManager::GetPluginBuilder(const std::string& target) const
{
    const auto found = m_PluginBuilders.find(target);
    return found != m_PluginBuilders.end() ? found->second : nullptr;
}
